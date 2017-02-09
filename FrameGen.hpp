//============================================================================
// Name        : FrameGen.hpp
// Author      : Milo Vermeulen
// Version     :
// Copyright   : Copyright (c) 2017 All rights reserved
// Description : FrameGen in C++, Ansi-style
//============================================================================

#ifndef FRAMEGEN_H_
#define FRAMEGEN_H_

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
#include <random>

#include "zlib.h"


class FrameGen {

private:
  // Frame data.
  std::string _path = "frames/";
  std::string _prefix = "test";
  std::string _suffix = "";
  std::string _extension = ".frame";

  unsigned int _frameNo = 0; // Total number of frames.

  // Frame contents.
  uint32_t _binaryData[120]; // Container for all data of a single frame.
  uint8_t _Stream_ID = 0; // Data type (trigger, calibration, unbiased, etc.) to route data.
  uint32_t _Reset_Count = 0; // Increments by one every 2^16 frames.
  uint32_t _WIB_Timestamp = 0; // Local timestamp.
  uint8_t _CrateNo = 0; // Crate number.
  uint8_t _SlotNo = 0; // Slot number.
  uint8_t _FiberNo = 0; // Fiber number.
  uint8_t _Capture = 0; // Error bit set if an error occurred during data capture.
  uint8_t _ASIC = 0; // Error bit set if an error was logged in ASIC data.
  uint8_t _WIB_Errors = 0; // Error bit set by the WIB.
  uint8_t _Checksum_A[4]; // Checksum of individual COLDATA blocks.
  uint8_t _Checksum_B[4]; // Checksum of individual COLDATA blocks.
  uint8_t _S1_Err[4] = {0,0,0,0}; // Indicate errors in capturing streams from COLDATA ASICS.
  uint8_t _S2_Err[4] = {0,0,0,0}; // Indicate errors in capturing streams from COLDATA ASICS.
  uint32_t _CRC32; // Complete checksum of frame.
  uint64_t _CRC32_Polynomial = 6186310019; // Polynomial to compute the CRC32 checksum (random 33-bit). Needs to be >=2^32.

  // Noise data.
  double _errProb = 0.00001; // Chance for any error bit to be set.
  uint16_t _noisePedestal = 250; // Pedestal of the noise (0 - 2^16).
  uint16_t _noiseAmplitude = 10; // Amplitude of the noise (0 - 2^16).

  // Random double generator to set error bits and generate noise.
  std::random_device _rd;
  std::mt19937 _mt;
  std::uniform_real_distribution<double> _randDouble;

  void fill(std::ofstream& frame);

  // Longitudinal redundancy check (8-bit).
  uint8_t checksum_A(unsigned int blockNum, uint8_t init = 0);
  // Modular checksum (8-bit).
  uint8_t checksum_B(unsigned int blockNum, uint8_t init = 0);
  // Cyclic redundancy check (32-bit).
  uint32_t CRC32(uint32_t padding = 0);

  // Print current frame.
  void printFrame();


public:

  // Constructors/destructors.
  FrameGen(): _mt(_rd()), _randDouble(0.0, 1.0){}
  FrameGen(const std::string& prefix): _prefix(prefix), _mt(_rd()), _randDouble(0.0, 1.0){}
  FrameGen(const int maxNoise): _noiseAmplitude(maxNoise){}
  ~FrameGen(){}

  // Frame name accessors/modifiers.
  void setPath(std::string path){ _path = path; }
  const std::string& getPath(){ return _path; }
  void setPrefix(std::string prefix){ _prefix = prefix; }
  const std::string& getPrefix(){ return _prefix; }
  void setSuffix(std::string suffix){ _suffix = suffix; }
  const std::string& getSuffix(){ return _suffix; }
  void setExtension(std::string extension){ _extension = extension; }
  const std::string& getExtension(){ return _extension; }

  const std::string getFrameName(unsigned long i){ return _path + _prefix + std::to_string(i) + _suffix + _extension; }
  const std::string getFrameName(){ return _path + _prefix + _suffix + _extension; }

  // Noise parameter accessors/modifiers.
  void setPedestal(uint16_t pedestal){ _noisePedestal = pedestal; }
  const uint16_t getPedestal(){ return _noisePedestal; }
  void setAmplitude(uint16_t amplitude){ _noiseAmplitude = amplitude; }
  const uint16_t getAmplitude(){ return _noiseAmplitude; }

  // Main generator function: builds frames and calls the fill function.
  void generate(const unsigned long Nframes = 1);
  void generate(const std::string& newPrefix, const unsigned long Nframes = 1);

  // Generator function to create a certain number of frames and place them in the same file.
  void generateSingleFile(const unsigned long Nframes = 1);
  void generateSingleFile(const std::string& newPrefix, const unsigned long Nframes = 1);

  // Function to check whether a frame corresponds to its checksums 
  // and whether any of its error bits are set. Overwrites _binaryData[]!
  const bool check(const std::string& framename);
  const bool check(){ return check(_path+_prefix+"0"+_suffix+_extension); }

  // Overloaded check functions to handle a range of files.
  const bool check(const unsigned int begin, const unsigned int end);
  const bool check(const unsigned int end);

  // Function to check frames within a single file.
  const bool checkSingleFile(const std::string& filename);
  const bool checkSingleFile(){ return checkSingleFile(getFrameName()); }

  // Function that attempts to open a file by its name, trying variations with class parameters (_extension, _path, etc.).
  const bool openFile(std::ifstream& strm, const std::string& filename);

  // Functions to compress and decompress frames or sets of frames by a file name.
  const bool compressFile(const std::string& filename);
  const bool compressFile(){ return compressFile(getFrameName()); }
  const bool decompressFile(const std::string& filename);
  const bool decompressFile(){ return decompressFile(getFrameName()); }

};

#endif /* FRAMEGEN_H_ */

