//============================================================================
// Name        : FrameGen.h
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
#include <ctime>
#include <random>

using namespace std;

static unsigned int _frameNo = 0; // Total number of frames.

class FrameGen {
private:
	// Frame data.
	string _path = "frames/";
	string _prefix = "test";
	string _suffix = "";
	string _extension = ".frame";

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

	// Metadata.
    double _errProb = 0.00001; // Chance for any error bit to be set.
    uint16_t _noisePedestal = 32; // Pedestal of the noise (0 - 2^16).
    uint16_t _noiseAmplitude = 8; // Amplitude of the noise (maximally 2^16).
    
	// Random double generator to set error bits and generate noise.
	random_device _rd;
	mt19937 _mt;
	uniform_real_distribution<double> _randDouble;

	void fill(ofstream& frame);

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
	FrameGen(): _mt(_rd()), _randDouble(0.0, 1.0) {}
	FrameGen(const string prefix): _mt(_rd()), _randDouble(0.0, 1.0), _prefix(prefix) {}
    FrameGen(const int maxNoise): _noiseAmplitude(maxNoise) {}
	~FrameGen() {}

	// Frame name accessors/modifiers.
	void setPath(string path)			{ _path = path; }
	const string getPath()				{ return _path; }
	void setPrefix(string prefix)		{ _prefix = prefix; }
	const string getPrefix()			{ return _prefix; }
	void setSuffix(string suffix)		{ _suffix = suffix; }
	const string getSuffix()			{ return _suffix; }
	void setExtension(string extension)	{ _extension = extension; }
	const string getExtension()			{ return _extension; }

	const string getFrameName(unsigned long i)	{ return _path + _prefix + to_string(i) + _suffix + _extension; }
	const string getFrameName()					{ return _path + _prefix + _suffix + _extension; }
    
    // Noise parameter accessors/modifiers.
    void setPedestal(uint16_t pedestal)     { _noisePedestal = pedestal; }
    const uint16_t getPedestal()            { return _noisePedestal; }
    void setAmplitude(uint16_t amplitude)	{ _noiseAmplitude = amplitude; }
    const uint16_t getAmplitude()           { return _noiseAmplitude; }

	// Main generator function: builds frames and calls the fill function.
	void generate(const unsigned long Nframes = 1);
    void generate(const string newPrefix, const unsigned long Nframes = 1);
    // Generator function to create a certain number of frames and place them in the same file.
    void generateSingleFile(const unsigned long Nframes = 1);
    void generateSingleFile(const string newPrefix, const unsigned long Nframes = 1);

    // Function to check whether a frame corresponds to its checksums and whether any of its error bits are set. Overwrites _binaryData[]!
    bool check(const string framename);
    bool check();
	// Overloaded check functions to handle a range of files.
	bool check(const unsigned int begin, const unsigned int end);
	bool check(const unsigned int end);
    // Function to check frames within a single file.
    bool checkSingleFile(const string filename);
    bool checkSingleFile();
};

#endif /* FILEGEN_H_ */