/* This file is part of Clementine.
   Copyright 2014-2015, Cultural Commons Collecting Society SCE mit
                        beschränkter Haftung (C3S SCE)
   Copyright 2014-2015, Thomas Mielke <thomas.mielke@c3s.cc>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "adorestreamer.h"

#include "core/logging.h"
#include "echoprint/Codegen.h"

#include <chromaprint.h>

// decide which fingerprinting algorithm to use:
//#define FINGERPRINTING_ALGORITHM_CHROMAPRINT
#define FINGERPRINTING_ALGORITHM_ECHOPRINT

AdoreStreamer::AdoreStreamer()
{
    probing_ = false;
    samplerate_ = 0;
#if defined(FINGERPRINTING_ALGORITHM_CHROMAPRINT)
    fingerprinting_algorithm_ = "chromaprint";
    fingerprinting_algorithm_version_ = QString::number(((int)CHROMAPRINT_ALGORITHM_DEFAULT));
#else // #if defined(FINGERPRINTING_ALGORITHM_ECHOPRINT)
    fingerprinting_algorithm_ = "echoprint";
    fingerprinting_algorithm_version_ = "4.12";
#endif
}

AdoreStreamer::~AdoreStreamer()
{
    engine_->RemoveBufferConsumer(this);
}

//! Pass the GStreamer Engine object to the class and get a `BufferConsumer` hook
void AdoreStreamer::SetEngine(GstEngine* engine) {
  engine_ = engine;
  engine_->AddBufferConsumer(this);
}

//! Callback for receiving audio buffers.
//! This member function collects audio data for MINIMUM_PROBING_DURATION seconds
//! after StartProbing() was initiated, then GetFingerprint() is being called.
//! @see StartProbing() @see SetEngine() for hooking @see ~AdoreStreamer() for unhooking
void AdoreStreamer::ConsumeBuffer(GstBuffer* buffer, int pipeline_id) {
  // gst_buffer_ref(buffer); -- dont ref it here, it already has been ref'ed in the engine for each BufferConsumer
  if (probing_)
  {
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ); // get physical access to the buffer memory

    const int samples_per_channel = map.size / sizeof(short) / 2;
    const int samplerate = (int)((((uint64_t)samples_per_channel * 10000000000) / buffer->duration) + 5) / 10;

    if (samplerate_ && samplerate_ != samplerate) // sample rate changed?
      StartProbing(); // force restart!
    samplerate_ = samplerate;

    // estimate necessary space so the buffer won't have to grow all the time
    QByteArray& buffer_qba = buffer_.buffer();
    if (!buffer_qba.capacity()) // just do this once per print
      buffer_qba.reserve(samplerate * sizeof(short) * 2 * MINIMUM_PROBING_DURATION * 6 / 5); // + 20% overhead

    // append GStreamer buffer to our fingerprinting buffer_
    buffer_.seek(buffer_.size());
    buffer_.write(reinterpret_cast<char*>(map.data), map.size);

    QTime now = QTime::currentTime();
    if (now >= probing_start_time.addSecs(MINIMUM_PROBING_DURATION))
    {
      StopProbing();
      GetFingerprint(fingerprint_);
      qLog(Debug) << "New chromaprint: " << fingerprint_;
    }

    gst_buffer_unmap(buffer, &map);
  }
  gst_buffer_unref(buffer); // already ref'ed in GstEnginePipeline
}

//! tell ConsumeBuffer() to start collecting audio buffers for later fingerprinting
void AdoreStreamer::StartProbing()
{
  if (probing_) StopProbing();
  probing_ = true;
  probing_start_time = QTime::currentTime();
  buffer_.buffer().clear();
  buffer_.open(QIODevice::WriteOnly);
}

//! stops collecting buffers and erases existing audio data from the fingerprinting buffer
void AdoreStreamer::StopProbing()
{
  if (probing_)
  {
    buffer_.close();
    probing_ = false;
  }
}


//--- private functions down here ---


/// This private member is called by GetFingerprint() and creates a mono mix
/// from stereo or multichannel audio data.
/// Note: target_buffer has to be source_numsamples / source_numberofchannels in size
void AdoreStreamer::MonoMix(const short *source_buffer, int source_numsamples, int source_numberofchannels, int source_samplingrate, short *target_buffer, int target_buffer_size)
{
  int source_index, target_index;
  // theoretically: const int target_buffer_size = source_numsamples / source_numberofchannels * 11025 / source_samplingrate;
  for (target_index = 0, source_index = 0;
    target_index < target_buffer_size && source_index < source_numsamples * source_numberofchannels;
    target_index++, source_index = (int)((int64_t)target_index * source_numberofchannels * source_samplingrate / 11025))
  {
    // mix stereo or multichannel source samples to mono
    int mixed_source_sample = 0;
    int channel;
    for (channel = 0; channel < source_numberofchannels; channel++)
      mixed_source_sample += (int)source_buffer[source_index + channel];
    mixed_source_sample /= source_numberofchannels;

    //qLog(Debug) << "sample " << target_index << "/" << source_index << " -- mixed " << mixed_source_sample << " from " << source_buffer[source_index] << " and " << source_buffer[source_index+1];

    target_buffer[target_index] = (short)mixed_source_sample;	// TODO: interpolation for source sampling rates that are no multiple of 11025
  }
}

/// After enough audio data has been collected in `buffer_` a fingerprint is being created from a mono downmix.
/// Currently, *EchoPrint* and *ChromaPrint* are implemented.
/// Just uncomment the corresponding `#define` at the start
/// of the cpp file. @see ChromaPrint() @see EchoPrint()
void AdoreStreamer::GetFingerprint(QString &fingerprint)
{
  QByteArray& buffer_qba = buffer_.buffer();
  if (!probing_ && buffer_qba.size())
  {
    short *monobuffer;
    int monobuffer_size;

    const int source_numsamples = buffer_.size() / sizeof(short) / 2;
    monobuffer_size = (int)((int64_t)source_numsamples * 11025 / 2 / samplerate_);
    monobuffer = new short[monobuffer_size];
    MonoMix(reinterpret_cast<const short*>(buffer_qba.data()), source_numsamples, 2, samplerate_, monobuffer, monobuffer_size);

#if defined(FINGERPRINTING_ALGORITHM_CHROMAPRINT)
    fingerprint = ChromaPrint(monobuffer, monobuffer_size);
#else // #if defined(FINGERPRINTING_ALGORITHM_ECHOPRINT)
    fingerprint = EchoPrint(monobuffer, monobuffer_size);
#endif

    delete monobuffer;
    buffer_.buffer().clear();
  }
  else
    fingerprint = "";
}

/// Creates a ChromaPrint audio fingerprint as used by MusicBrainz.
/// The parameter `size` holds the number of shorts in `data`.
QString AdoreStreamer::ChromaPrint(short *data, int size)
{
  ChromaprintContext* chromaprint =
      chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
  chromaprint_start(chromaprint, 11025, 1);
  chromaprint_feed(chromaprint, reinterpret_cast<void*>(data), size);
  chromaprint_finish(chromaprint);

  void* fprint = nullptr;
  int fprint_size = 0;
  int ret = chromaprint_get_raw_fingerprint(chromaprint, &fprint, &fprint_size);
  QByteArray fingerprint;
  if (ret == 1) {
    void* encoded = nullptr;
    int encoded_size = 0;
    chromaprint_encode_fingerprint(fprint, fprint_size, CHROMAPRINT_ALGORITHM_DEFAULT,
                                   &encoded, &encoded_size, 1);

    fingerprint.append(reinterpret_cast<char*>(encoded), encoded_size);

    chromaprint_dealloc(fprint);
    chromaprint_dealloc(encoded);
  }
  chromaprint_free(chromaprint);

  return QString(fingerprint);
}

/// Creates a EchoPrint audio fingerprint as used by EchoNest/last.fm.
/// The parameter `size` holds the number of shorts in `data`.
/// Note that the short int samples in the mono buffer have to be
/// converted to floats before passing to the fingerprinting algorithm.
QString AdoreStreamer::EchoPrint(short *data, int size)
{
  // first, convert short ints to floats
  int i;
  float *floatbuffer = new float[size];
  for (i = 0; i < size; i++)
  {
    floatbuffer[i] = (float)data[i] / 32768.0;
  }

  // create fingerprint (code)
  Codegen g(floatbuffer, size, 0);

  delete floatbuffer;

  return QString::fromStdString(g.getCodeString());
}
