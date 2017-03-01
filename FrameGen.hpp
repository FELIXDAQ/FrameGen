//============================================================================
// Name        : FrameGen.hpp
// Author      : Milo Vermeulen
// Version     :
// Copyright   : Copyright (c) 2017 All rights reserved
// Description : FrameGen in C++, Ansi-style
//============================================================================

#ifndef FRAMEGEN_HPP_
#define FRAMEGEN_HPP_

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
#include <random>

#include "zlib.h"

#define CRC32_POLYNOMIAL 6186310019 // Polynomial to compute the CRC32 checksum (random 33-bit). Needs to be >=2^32.

namespace framegen {
    
    class Frame {
        // Frame structure from Eric Hazen's preliminary WIB->FELIX frame format:
        // http://docs.dunescience.org/cgi-bin/RetrieveFile?docid=1701&filename=ProtoDUNE_to_FELIX.pdf&version=1
    private:
        uint32_t _binaryData[120]; // Container for all data of a single frame.
        //    uint64_t _CRC32_Polynomial = 6186310019; // Polynomial to compute the CRC32 checksum (random 33-bit). Needs to be >=2^32.
    public:
        // Header/footer accessors.
        const uint8_t  getStreamID()        { return _binaryData[0]; } // Data type (trigger, calibration, unbiased, etc.) to route data.
        const uint32_t getResetCount()      { return _binaryData[0]>>8; } // Increments by one every 2^16 frames.
        const uint32_t getWIBTimestamp()    { return _binaryData[1]; } // Local timestamp.
        const uint8_t  getCrateNo()         { return _binaryData[2]&0x1F; } // Crate number.
        const uint8_t  getSlotNo()          { return (_binaryData[2]>>5)&0x7; } // Slot number.
        const uint8_t  getFiberNo()         { return (_binaryData[2]>>8)&0x7; } // Fiber number.
        const uint8_t  getCapture()         { return (_binaryData[2]>>16)&0xF; } // Error bit set if an error occurred during data capture.
        const uint8_t  getASIC()            { return (_binaryData[2]>>20)&0xF; } // Error bit set if an error was logged in ASIC data.
        const uint8_t  getWIBErrors()       { return _binaryData[2]>>16; } // Error bit set by the WIB.
        const uint32_t getCRC32()           { return _binaryData[115]; } // Complete checksum of frame.
        // COLDATA blocks accessors.
        const uint8_t  getChecksumA(int blockNum)           { return _binaryData[3+28*blockNum]; } // Checksum of individual COLDATA blocks.
        const uint8_t  getChecksumB(int blockNum)           { return _binaryData[3+28*blockNum]>>8; } // Checksum of individual COLDATA blocks.
        const uint8_t  getS1Err(int blockNum)               { return (_binaryData[3+28*blockNum]>>16)&0xF; } // Indicate errors in capturing streams from COLDATA ASICS.
        const uint8_t  getS2Err(int blockNum)               { return (_binaryData[3+28*blockNum]>>20)&0xF; } // Indicate errors in capturing streams from COLDATA ASICS.
        const uint16_t getCOLDATA(int blockNum, int num)    { return _binaryData[4+28*blockNum+num/2]>>(16*(1-num%2)); } // Raw data.
        
        // Header/footer mutators.
        void setStreamID(uint8_t newStreamID)           { _binaryData[0] = (_binaryData[0]&~(0xFF)) | newStreamID; }
        void setResetCount(uint32_t newResetCount)      { _binaryData[0] = (_binaryData[0]&0xFF) | newResetCount<<8; }
        void setWIBTimestamp(uint32_t newWIBTimestamp)  { _binaryData[1] = newWIBTimestamp; }
        void setCrateNo(uint8_t newCrateNo)             { _binaryData[2] = (_binaryData[2]&~(0x1F)) | (newCrateNo&0x1F); }
        void setSlotNo(uint8_t newSlotNo)               { _binaryData[2] = (_binaryData[2]&~(0x7<<5)) | (newSlotNo&0x7)<<5; }
        void setFiberNo(uint8_t newFiberNo)             { _binaryData[2] = (_binaryData[2]&~(0x7<<8)) | (newFiberNo&0x7)<<8; }
        void setCapture(uint8_t newCapture)             { _binaryData[2] = (_binaryData[2]&~(0xF<<16)) | (newCapture&0xF)<<16; }
        void setASIC(uint8_t newASIC)                   { _binaryData[2] = (_binaryData[2]&~(0xF<<20)) | (newASIC&0xF)<<20; }
        void setWIBErrors(uint8_t newWIBErrors)         { _binaryData[2] = (_binaryData[2]&~(0xFF<<24)) | newWIBErrors<<24; }
        void setCRC32(uint32_t newCRC32)                { _binaryData[115] = newCRC32; }
        // COLDATA blocks mutators.
        void setChecksumA(int blockNum, uint8_t newChecksumA)       { _binaryData[3+28*blockNum] = (_binaryData[3+28*blockNum]&~(0xFF)) | newChecksumA; }
        void setChecksumB(int blockNum, uint8_t newChecksumB)       { _binaryData[3+28*blockNum] = (_binaryData[3+28*blockNum]&~(0xFF<<8)) | newChecksumB<<8; }
        void setS1Err(int blockNum, uint8_t newS1Err)               { _binaryData[3+28*blockNum] = (_binaryData[3+28*blockNum]&~(0xF<<16)) | (newS1Err&0xF)<<16; }
        void setS2Err(int blockNum, uint8_t newS2Err)               { _binaryData[3+28*blockNum] = (_binaryData[3+28*blockNum]&~(0xF<<20)) | (newS2Err&0xF)<<20; }
        void setCOLDATA(int blockNum, int num, uint16_t newCOLDATA) { _binaryData[4+28*blockNum+num/2] = (_binaryData[4+28*blockNum+num/2]&~(0xFFFF<<(16*num%2))) | newCOLDATA<<(16*num%2); }
        
        bool load(const std::string& filename, int frameNum = 0);
        void load(std::ifstream& strm, int frameNum = 0);
        bool print(const std::string& filename);
        bool print(std::ofstream& strm);
        
        // Longitudinal redundancy check (8-bit).
        uint8_t checksum_A(unsigned int blockNum, uint8_t init = 0);
        // Modular checksum (8-bit).
        uint8_t checksum_B(unsigned int blockNum, uint8_t init = 0);
        // Cyclic redundancy check (32-bit).
        uint32_t CRC32(uint32_t padding = 0, uint64_t CRC32_Polynomial = CRC32_POLYNOMIAL);
    };
    
    // Function to check whether a frame corresponds to its checksums.
    const bool check(const std::string& filename);
    // Function to check frames within a single file.
    const bool checkSingleFile(const std::string& filename);
    
    // Functions to compress and decompress frames or sets of frames by a file name.
    const bool compressFile(const std::string& filename);
    const bool decompressFile(const std::string& filename);
    
    class FrameGen {
    private:
        // File data.
        std::string _path = "frames/";
        std::string _prefix = "test";
        std::string _suffix = "";
        std::string _extension = ".frame";
        
        unsigned int _frameNo = 0; // Total number of frames.
        
        // Frame contents.
        Frame _frame;
        
        // Noise data.
        double _errProb = 0.00001; // Chance for any error bit to be set.
        uint16_t _noisePedestal = 250; // Pedestal of the noise (0 - 2^16).
        uint16_t _noiseAmplitude = 10; // Amplitude of the noise (0 - 2^16).
        
        // Random double generator to set error bits and generate noise.
        std::random_device _rd;
        std::mt19937 _mt;
        std::uniform_real_distribution<double> _randDouble;
        
        void fill(std::ofstream& frame);
        
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
        
        const std::string getFileName(unsigned long i){ return _path + _prefix + std::to_string(i) + _suffix + _extension; }
        const std::string getFileName(){ return _path + _prefix + _suffix + _extension; }
        
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
        
        // Function that attempts to open a file by its name, trying variations with class parameters (_extension, _path, etc.).
        const bool openFile(std::ifstream& strm, const std::string& filename);
        
        // Overloaded and ranged check functions that absolutely require the FrameGen filename parameters.
        const bool check() { return framegen::check(_path+_prefix+"0"+_suffix+_extension); }
        const bool check(const unsigned int begin, const unsigned int end);
        const bool check(const unsigned int end) { return check(0,end); }
        const bool checkSingleFile() { return framegen::checkSingleFile(getFileName()); }
        
        // Overloaded compression/decompression functions.
        const bool compressFile() { return framegen::compressFile(getFileName()); }
        const bool decompressFile() { return framegen::decompressFile(getFileName()); }
    };
    
} // namespace framegen

#endif /* FRAMEGEN_H_ */

