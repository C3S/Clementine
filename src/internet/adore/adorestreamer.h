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
#ifndef ADORESTREAMER_H
#define ADORESTREAMER_H

#include "engines/gstengine.h"
#include "engines/bufferconsumer.h"

#include <QBuffer>
#include <QTime>

#define MINIMUM_PROBING_DURATION 40 ///< seconds to take a fingerprint

/*! This class handles the received audio buffers. It computes the
 *  sample rate, converts the stereo stream to mono, and draws
 *  a fingerprint from the probed audio data.
 */

class AdoreStreamer : public BufferConsumer
{
public:
  AdoreStreamer();
  ~AdoreStreamer();

  void SetEngine(GstEngine* engine);
  void ConsumeBuffer(GstBuffer* buffer, int pipeline_id);
  void StartProbing();
  void StopProbing();
  inline QString& GetLastFingerprint() { return fingerprint_; } ///< public read-only access to the private `fingerprint_` member variable.
  inline QString& GetFingerprintingAlgorithm() { return fingerprinting_algorithm_; }  ///< public read-only access to the private `fingerprinting_algorithm_` member variable.
  inline QString& GetFingerprintingAlgorithmVersion() { return fingerprinting_algorithm_version_; } ///< public read-only access to the private `fingerprinting_algorithm_version_` member variable.
  inline void ResetLastFingerprint() { fingerprint_ = ""; } ///< erases a previously created fingerprint. @see GetLastFingerprint()

private:
  void MonoMix(const short *source_buffer, int source_numsamples, int source_numberofchannels, int source_samplingrate, short *target_buffer, int target_buffer_size); 
  void GetFingerprint(QString &fingerprint);
  QString ChromaPrint(short *data, int size);
  QString EchoPrint(short *data, int size);

  GstEngine* engine_;         ///< the GStreamer engine object, necessary to peek into the audio stream
  QBuffer buffer_;            ///< fingerprinting buffer to collect audio buffers for some seconds before creating a fingerprint
  bool probing_;              ///< controls the collection. @see StartProbing() @see StopProbing()
  QTime probing_start_time;   ///< used to measure when MINIMUM_PROBING_DURATION has been reached and then create a fingerprint
  int samplerate_;            ///< computed in ConsumeBuffer() and stored to determine whether the rate has changed and the audio buffer collection needs a restart. @see ConsumeBuffer()
  QString fingerprint_;       ///< ascii (base64) representation of the result created by GetFingerprint(). @see GetFingerprint() @see GetLastFingerprint()
  QString fingerprinting_algorithm_;          ///< either 'echoprint' or 'chromapint', depending on the `#define` uncommented in the cpp file. This is set in the constructor. @see GetFingerprintingAlgorithm()
  QString fingerprinting_algorithm_version_;  ///< current version of the fingerprinting algorithm. This is set in the constructor. @see GetFingerprintingAlgorithmVersion()
};

#endif // ADORESTREAMER_H
