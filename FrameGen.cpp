//============================================================================
// Name        : FrameGen.cpp
// Author      : Milo Vermeulen
// Version     :
// Copyright   : Copyright (c) 2017 All rights reserved
// Description : FrameGen in C++, Ansi-style
//============================================================================

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
    _Capture = _randDouble(_mt)<_errProb;
    _ASIC = _randDouble(_mt)<_errProb;
    _WIB_Errors = _randDouble(_mt)<_errProb;
    _binaryData[2] = _CrateNo + (_SlotNo<<5) + (_FiberNo<<8) + (_Capture<<16) + (_ASIC<<20) + (_WIB_Errors<<24);

    // Produce four COLDATA blocks. (Random numbers have 16 bits each.)
    binomial_distribution<int> _randInt(_noiseAmplitude*2, 0.5);
    int randnum = 0; // Temporary variable to check random noise generation.
    for(int i=0; i<4; i++) {
        // Build 27 32-bit words of COLDATA in sets of two 16-bit numbers.
        for(int j=0; j<27; j++)
        for(int k=0; k<2; k++) {
            do {
                randnum = _randInt(_mt)-_noiseAmplitude+_noisePedestal;
            } while(randnum<0);
            _binaryData[4+i*28+j] += randnum<<(16*k);
        }

        // Generate checksums and insert them.
        _Checksum_A[i] = checksum_A(i);
        _Checksum_B[i] = checksum_B(i);
        _S1_Err[i] = _randDouble(_mt)<_errProb;
        _S2_Err[i] = _randDouble(_mt)<_errProb;
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
const bool FrameGen::check(const string framename) {
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

    // Check errors and produce a warning.
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

// Overloaded check functions to handle a range of files.
const bool FrameGen::check(const unsigned int begin, const unsigned int end) {
    if(_path == "")
    cout << "Checking frames." << endl;
    else
    cout << "Checking frames at path " << _path << "." << endl;
    for(unsigned int i=begin; i<end; i++)
    if(!check(getFrameName(i)))
    return false;
    return true;
}
const bool FrameGen::check(const unsigned int end) {
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
const bool FrameGen::checkSingleFile(const string filename) {
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

// Function that attempts to open a file by its name, trying variations with class parameters (_extension, _path, etc.).
const bool FrameGen::openFile(ifstream& strm, const string& filename){
    string possibleName[5];
    possibleName[0] = filename;
    possibleName[1] = filename+_extension;
    possibleName[2] = _path+filename+_extension;
    possibleName[3] = filename+_suffix+_extension;
    possibleName[4] = _path+filename+_suffix+_extension;

    for(int i=0; !strm && i<5; i++)
    strm.open(possibleName[i], ios::binary);

    return strm? true: false;
}

// Function to compress frames or sets of frames.
const bool FrameGen::compressFile(const string filename) {
    // Open original file.
    ifstream ifcomp(filename, ios::binary);
    if(!openFile(ifcomp, filename)) {
        cout << "Error (compress()): file " << filename << " could not be openend." << endl;
        return false;
    }

    // Get file length and create buffer (to be changed to shifting window).
    ifcomp.seekg(0,ifcomp.end);
    unsigned long iflength = ifcomp.tellg();
    ifcomp.seekg(0,ifcomp.beg);

    Bytef* ifbuff = new Bytef[iflength];
    ifcomp.read((char*)ifbuff,iflength);

    // Create file to output compressed data into.
    ofstream ofcomp(filename+".comp", ios::binary);
    if(!ofcomp) {
        cout << "Error (compress()): file " << filename+".comp" << " could not be created." << endl;
        return false;
    }

    // Create buffer for compressed file and perform the compression (with the compression function from zlib).
    unsigned long oflength = iflength*1.1 + 12; // The new buffer needs to be longer than the original only until compression is complete.
    Bytef* ofbuff = new Bytef[oflength];
    switch(::compress(ofbuff, &oflength, ifbuff, iflength)) {
        case Z_OK:          break;
        case Z_MEM_ERROR:   cout << "Error (compress()): out of memory." << endl;                   return false;
        case Z_BUF_ERROR:   cout << "Error (compress()): output buffer not large enough." << endl;  return false;
        case Z_DATA_ERROR:  cout << "Error (compress()): the data was corrupted." << endl;          return false;
        default:            cout << "Error (compress()): unknown error code." << endl;              return false;
    }

    ofcomp.write((char*)ofbuff,oflength);

    // Cleanup.
    delete[] ifbuff;
    delete[] ofbuff;
    remove(filename.c_str());
    ifcomp.close();
    ofcomp.close();

    return true;
}

const bool FrameGen::decompressFile(const string filename) {
    // Check whether the filename has the right extension.
    bool compExt = true;
    if(strcmp(filename.substr(filename.rfind("."),filename.length()).c_str(), ".comp")) { // Compare filename extension to ".comp".
    cout << "Warning: the file " << filename << " does not have the default \".comp\" extension." << endl;
    compExt = false;
}
// Open original file.
ifstream ifdecomp(filename, ios::binary);
if(!ifdecomp) {
    cout << "Error (decompress()): file " << filename << " could not be opened." << endl;
    return false;
}

// Get file length and create buffer (to be changed to shifting window).
ifdecomp.seekg(0,ifdecomp.end);
unsigned long iflength = ifdecomp.tellg();
ifdecomp.seekg(0,ifdecomp.beg);

Bytef* ifbuff = new Bytef[iflength];
ifdecomp.read((char*)ifbuff,iflength);

// Create file to output compressed data into.
ofstream ofdecomp(compExt? filename.substr(0,filename.length()-5): filename);
if(!ofdecomp) {
    cout << "Error (decompress()): file " << (compExt? filename.substr(0,filename.length()-5): filename) << " could not be created." << endl;
    return false;
}

// Create buffer for decompressed file and perform the decompression (with the decompression function from zlib).
unsigned long oflength = iflength*30; // Just to be safe.
Bytef* ofbuff = new Bytef[oflength];
switch(::uncompress(ofbuff, &oflength, ifbuff, iflength)) {
    case Z_OK:          break;
    case Z_MEM_ERROR:   cout << "Error (decompress()): out of memory." << endl;                     return false;
    case Z_BUF_ERROR:   cout << "Error (decompress()): output buffer not large enough." << endl;    return false;
    case Z_DATA_ERROR:  cout << "Error (decompress()): the data was corrupted." << endl;            return false;
    default:            cout << "Error (decompress()): unknown error code." << endl;                return false;
}

ofdecomp.write((char*)ofbuff,oflength);

// Cleanup.
delete[] ifbuff;
delete[] ofbuff;
remove(filename.c_str());
ifdecomp.close();
ofdecomp.close();

return true;
}
