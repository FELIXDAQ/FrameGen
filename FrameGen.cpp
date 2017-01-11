//============================================================================
// Name        : FrameGen.cpp
// Author      : Milo Vermeulen
// Version     :
// Copyright   : Copyright (c) 2017 All rights reserved
// Description : FrameGen in C++, Ansi-style
//============================================================================

#include <iostream>
#include <string>

#include "FrameGen.h"

using namespace std;

void FrameGen::fill(ofstream& frame) {
	// Reset _binaryData[].
	for(int i=0; i<120; i++)
		_binaryData[i] = 0;

	// Header.
	_Stream_ID = rand()%8;
	_Reset_Count = rand()%16777216; // (2^24)
	_binaryData[0] = _Stream_ID + (_Reset_Count<<8);

	_WIB_Timestamp = time(nullptr);
	_binaryData[1] = _WIB_Timestamp;

	_CrateNo = rand()%32;
	_SlotNo = rand()%8;
	_FiberNo = rand()%8;
	_Capture = _rand(_mt)<_errProb;
	_ASIC = _rand(_mt)<_errProb;
	_WIB_Errors = _rand(_mt)<_errProb;
	_binaryData[2] = _CrateNo + (_SlotNo<<5) + (_FiberNo<<8) + (_Capture<<16) + (_ASIC<<20) + (_WIB_Errors<<24);
    
    // Check whether _maxNoise has a valid value.
    if(_maxNoise<0 || _maxNoise>=65536) {
        cout << "Warning: noise amplitude out of range (0 - 2^16). Value reset." << endl;
        _maxNoise = 8;
    }
    
	// Produce four COLDATA blocks. (Random numbers have 16 bits each.)
	for(int i=0; i<4; i++) {
		// Build 27 32-bit words of COLDATA.
		for(int j=0; j<27; j++)
            _binaryData[4+i*28+j] = rand()%_maxNoise + ((rand()%_maxNoise)<<16);

		// Generate checksums and insert them.
		_Checksum_A[i] = checksum_A(i);
		_Checksum_B[i] = checksum_B(i);
		_S1_Err[i] = _rand(_mt)<_errProb;
		_S2_Err[i] = _rand(_mt)<_errProb;
		_binaryData[3+i*28] = _Checksum_A[i] + (_Checksum_B[i]<<8) + (_S1_Err[i]<<16) + (_S2_Err[i]<<20);
	}

	// Final checksum over the entire frame.
	_CRC32 = CRC32();
	_binaryData[115] = _CRC32;

	// Print the generated data to frame.
	for(int i=0; i<120; i++) {
		frame << (unsigned char)((_binaryData[i]<<24)>>24);
		frame << (unsigned char)((_binaryData[i]<<16)>>24);
		frame << (unsigned char)((_binaryData[i]<<8)>>24);
		frame << (unsigned char)(_binaryData[i]>>24);
	}
}

// Longitudinal redundancy check (8-bit).
uint8_t FrameGen::checksum_A(unsigned int blockNum, uint8_t init) {
	if(blockNum>3) {
		cout << "Error: invalid block number passed to checksum_A(). (Valid range: 0-3.)" << endl;
		return 0;
	}
	uint8_t result = init;
	for(unsigned int i=4+blockNum*28; i<4+blockNum*28+27; i++) {
		// Divide 32-bit into 8-bit and XOR.
		result ^= (_binaryData[i]<<24)>>24;
		result ^= (_binaryData[i]<<16)>>24;
		result ^= (_binaryData[i]<<8)>>24;
		result ^= _binaryData[i]>>24;
	}
	return result;
}

// Modular checksum (8-bit).
uint8_t FrameGen::checksum_B(unsigned int blockNum, uint8_t init) {
	if(blockNum>3) {
		cout << "Error: invalid block number passed to checksum_B(). (Valid range: 0-3.)" << endl;
		return 0;
	}
	uint8_t result = init;
	for(unsigned int i=4+blockNum*28; i<4+blockNum*28+27; i++) {
		// Divide 32-bit into 8-bit and add.
		result += (_binaryData[i]<<24)>>24;
		result += (_binaryData[i]<<16)>>24;
		result += (_binaryData[i]<<8)>>24;
		result += _binaryData[i]>>24;
	}
	return -result;
}

// Cyclic redundancy check (32-bit).
uint32_t FrameGen::CRC32(uint32_t padding) {
	uint64_t shiftReg = (_binaryData[0]<<1) + ((_binaryData[1]>>31)&1); // Shifting register.
	// Shift through the data.
	for(int i=0; i<114*32; i++) { // The register shifts through 115 32-bit words and is 33 bits long.
		// Perform XOR on the shifting register if the leading bit is 1 and shift.
		if(shiftReg & 8589934592) // (2^33)
			shiftReg ^= _CRC32_Polynomial;
		shiftReg = shiftReg<<1 | ((_binaryData[i/32+1]>>(31-(i%32)))&1);
	}
	// Shift through padding.
	for(int i=0; i<32; i++) {
		if(shiftReg & 8589934592)// (2^33)
			shiftReg ^= _CRC32_Polynomial;
		shiftReg = shiftReg<<1 | ((padding>>(31-i))&1);
	}
	// One last XOR after the final shift.
	if(shiftReg & 8589934592) // (2^33)
		shiftReg ^= _CRC32_Polynomial;

	return shiftReg;
}

// Print current frame.
void FrameGen::printFrame() {
	for(int i=0; i<120; i++)
		cout << _binaryData[i] << endl;
}

// Main generator function: builds frames and calls the fill function.
void FrameGen::generate(const unsigned long Nframes) {
	if(_path == "")
		cout << "Generating frames." << endl;
	else
		cout << "Generating frames at path " << _path << "." << endl;
	for(unsigned long i=0; i<Nframes; i++) {
		// Open frame, fill it and close it.
		ofstream frame(getFrameName(i));
		if(!frame) {
			cout << "Error (generate()): " << getFrameName(i) << " could not be opened." << endl;
            cout << "\t(You will have to create the path " << _path << " if you haven't already.)" << endl;
			return;
		}
		fill(frame);
		frame.close();

		// Keep track of progress.
		if((i*100)%Nframes==0)
			cout << i*100/Nframes << "%\r" << flush;
		_frameNo++;
	}
	cout << "    \tDone." << endl;
}
// Overloaded generate function to handle new prefixes.
void FrameGen::generate(const string newPrefix, const unsigned long Nframes) {
    _prefix = newPrefix;
    generate(Nframes);
}

// Generator function to create a certain number of frames and place them in the same file.
void FrameGen::generateSingleFile(const unsigned long Nframes) {
	if(_path == "")
		cout << "Generating frames." << endl;
	else
		cout << "Generating frames at path " << _path << "." << endl;

	// Open frame, fill it and close it.
	string filename = _path+_prefix+_suffix+_extension;
	ofstream file(filename);
	if(!file) {
		cout << "Error (generate()): " << filename << " could not be opened." << endl;
		return;
	}
	for(unsigned long i=0; i<Nframes; i++) {
		fill(file);

		// Keep track of progress.
		if((i*100)%Nframes==0)
			cout << i*100/Nframes << "%\r" << flush;
		_frameNo++;
	}
	file.close();
	cout << "    \tDone." << endl;
}
// Overloaded generate function to handle new prefixes.
void FrameGen::generateSingleFile(const string newPrefix, const unsigned long Nframes) {
    _prefix = newPrefix;
    generateSingleFile(Nframes);
}

// Function to check whether a frame corresponds to its checksums and whether any of its error bits are set. Overwrites _binaryData[]!
bool FrameGen::check(const string framename) {
	// Open frame.
	ifstream frame(framename, ios::binary);
	if(!frame) {
		cout << "Error (check()): could not open frame " << framename << "." << endl;
		return false;
	}

	// Load data from frame (32 bits = 4 bytes at a time).
	for(int i=0; i<120; i++)
		frame.read((char*)&(_binaryData[i]),4);

	// Check checksums.
	for(int i=0; i<4; i++) {
		if(checksum_A(i, _binaryData[3+i*28]))
			cout << "Frame " << framename << ", COLDATA block " << i+1 << "/4 contains an error in checksum A." << endl;
		if(checksum_B(i, _binaryData[3+i*28]>>8))
			cout << "Frame " << framename << ", COLDATA block " << i+1 << "/4 contains an error in checksum B." << endl;
	}
	if(CRC32(_binaryData[115])) {
		cout << "Frame " << framename << " failed its cyclic redundancy check." << endl;
		frame.close();
		return false;
	}

	// Check errors.
	if(_binaryData[2]>>16 & 1) // Capture
		cout << "Warning: Capture error bit set in frame " << framename << "." << endl;
	if(_binaryData[2]>>20 & 1) // ASIC
		cout << "Warning: ASIC error bit set in frame " << framename << "." << endl;
	if(_binaryData[2]>>24 & 1) // WIB_Errors
		cout << "Warning: WIB error bit set in frame " << framename << "." << endl;
	for(int i=0; i<4; i++) {
		if(_binaryData[3+i*28]>>16 & 1) // S1
			cout << "Warning: S1 error bit set in frame " << framename << ", block " << i+1 << "/4." << endl;
		if(_binaryData[3+i*28]>>20 & 1) // S2
			cout << "Warning: S2 error bit set in frame " << framename << ", block " << i+1 << "/4." << endl;
	}

	frame.close();
	return true;
}
// Overloaded check function to allow for default argument from class parameters.
bool FrameGen::check() {
    return check(_path+_prefix+"0"+_suffix+_extension);
}

// Overloaded check functions to handle a range of files.
bool FrameGen::check(const unsigned int begin, const unsigned int end) {
	if(_path == "")
		cout << "Checking frames." << endl;
	else
		cout << "Checking frames at path " << _path << "." << endl;
	for(unsigned int i=begin; i<end; i++)
		if(!check(getFrameName(i)))
			return false;
	return true;
}
bool FrameGen::check(const unsigned int end) {
	if(_path == "")
		cout << "Checking frames." << endl;
	else
		cout << "Checking frames at path " << _path << "." << endl;
	for(unsigned int i=0; i<end; i++)
		if(!check(getFrameName(i)))
			return false;
	return true;
}

// Function to check files within a single file.
bool FrameGen::checkSingleFile(const string filename) {
	// Open file.
	ifstream file(filename, ios::binary);
	if(!file) {
		cout << "Error (check()): could not open file " << filename << "." << endl;
		return false;
    }
    if(_path == "")
        cout << "Checking frames." << endl;
    else
        cout << "Checking frames at " << getFrameName() << "." << endl;

	// Get number of frames and check whether this is an integer.
	file.seekg(0,file.end);
	if(file.tellg()%480) {
		cout << "Error: file " << filename << " contains unreadable frames." << endl;
		return false;
	}
	int numberOfFrames = file.tellg()/480;
	file.seekg(0,file.beg);

	bool result = true;

	for(int j=0; j<numberOfFrames; j++) {
		// Load data from file (32 bits = 4 bytes at a time).
		for(int i=0; i<120; i++)
			file.read((char*)&(_binaryData[i]),4);

		// Check checksums.
		for(int i=0; i<4; i++) {
			if(checksum_A(i, _binaryData[3+i*28])) {
				cout << "Frame " << j+1 << "/" << numberOfFrames << ", COLDATA block " << i+1 << "/4 contains an error in checksum A." << endl;
				result = false;
			}
			if(checksum_B(i, _binaryData[3+i*28]>>8)) {
				cout << "Frame " << j+1 << "/" << numberOfFrames << ", COLDATA block " << i+1 << "/4 contains an error in checksum B." << endl;
				result = false;
			}
		}
		if(CRC32(_binaryData[115])) {
			cout << "Frame " << filename << " failed its cyclic redundancy check." << endl;
			file.close();
			result = false;
		}

		// Check errors.
		if(_binaryData[2]>>16 & 1) // Capture
			cout << "Warning: Capture error bit set in frame " << j+1 << "/" << numberOfFrames << "." << endl;
		if(_binaryData[2]>>20 & 1) // ASIC
			cout << "Warning: ASIC error bit set in frame " << j+1 << "/" << numberOfFrames << "." << endl;
		if(_binaryData[2]>>24 & 1) // WIB_Errors
			cout << "Warning: WIB error bit set in frame " << j+1 << "/" << numberOfFrames << "." << endl;
		for(int i=0; i<4; i++) {
			if(_binaryData[3+i*28]>>16 & 1) // S1
				cout << "Warning: S1 error bit set in frame " << j+1 << "/" << numberOfFrames << ", block " << i+1 << "/4." << endl;
			if(_binaryData[3+i*28]>>20 & 1) // S2
				cout << "Warning: S2 error bit set in frame " << j+1 << "/" << numberOfFrames << ", block " << i+1 << "/4." << endl;
		}
	}

	file.close();
	return result;
}
// Overloaded function to allow for default argument from class parameters.
bool FrameGen::checkSingleFile() {
    return checkSingleFile(getFrameName());
}
