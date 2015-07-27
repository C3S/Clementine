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
#ifndef C3SIMPSTREAMER_H
#define C3SIMPSTREAMER_H

#include "engines/gstengine.h"
#include "engines/bufferconsumer.h"

#include <QBuffer>
#include <QTime>

// this class handles the received audio buffers, computes the sample rate, and converts the stereo stream to mono

#define MINIMUM_PROBING_DURATION 40 // seconds to take a fingerprint

class C3sImpStreamer : public BufferConsumer
{
public:
  C3sImpStreamer();
  ~C3sImpStreamer();

  void SetEngine(GstEngine* engine);
  void ConsumeBuffer(GstBuffer* buffer, int pipeline_id);
  void StartProbing();
  void StopProbing();
  inline QString& GetLastFingerprint() { return fingerprint_; }
  inline QString& GetFingerprintingAlgorithm() { return fingerprinting_algorithm_; }
  inline QString& GetFingerprintingAlgorithmVersion() { return fingerprinting_algorithm_version_; }
  inline void ResetLastFingerprint() { fingerprint_ = ""; }

private:
  void MonoMix(const short *source_buffer, int source_numsamples, int source_numberofchannels, int source_samplingrate, short *target_buffer, int target_buffer_size); 
  void GetFingerprint(QString &fingerprint);
  QString ChromaPrint(short *data, int size);

  GstEngine* engine_;
  QBuffer buffer_;
  bool probing_;
  QTime probing_start_time;
  int samplerate_;
  QString fingerprint_;
  QString fingerprinting_algorithm_;
  QString fingerprinting_algorithm_version_;
};

#endif // C3SIMPSTREAMER_H
