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
#include <iomanip>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
#include <random>
#include <chrono>

#include "zlib.h"

#define CRC32_POLYNOMIAL 3988292384//7976584769//4374732215//6186310019 // Polynomial to compute the CRC32 checksum (random 33-bit). Needs to be >=2^32.

namespace framegen {
    
    static uint64_t numberOfFrames = 0;
    
    const uint32_t getBitRange(const uint32_t& word, int begin, int end) {
        if(begin==0 && end==31)
            return word;
        else
            return (word>>begin)&((1<<(end-begin+1))-1);
    };
    
    const uint8_t getBit(const uint32_t& word, int num) { return (word>>num)&1; };
    
    template <typename T>
    void setBitRange(uint32_t& word, T& newValue, int begin, int end) {
        if(begin==0 && end==31) {
            word = newValue;
            return;
        }
        uint32_t mask = (1<<(end-begin+1))-1;
        word = (word&~(mask<<begin)) | ((newValue&mask)<<begin);
    };
    
    struct WIB_header {
        // Header/footer accessors.
        uint32_t _binaryData[4];
        const uint8_t  getStreamID()        { return getBitRange(_binaryData[0],0,7); }     // Data type (trigger, calibration, unbiased, etc.) to route data.
        const uint32_t getResetCount()      { return getBitRange(_binaryData[0],8,31); }    // Increments by one every 2^16 frames.
        
        const uint32_t getWIBTimestamp()    { return getBitRange(_binaryData[1],0,31); }    // Local timestamp.
        
        const uint8_t  getCrateNo()         { return getBitRange(_binaryData[2],0,4); }     // Crate number.
        const uint8_t  getSlotNo()          { return getBitRange(_binaryData[2],5,7); }     // Slot number.
        const uint8_t  getFiberNo()         { return getBitRange(_binaryData[2],8,10); }    // Fiber number.
        const uint8_t  getCapture()         { return getBitRange(_binaryData[2],16,19); }   // Error bit set if an error occurred during data capture.
        const uint8_t  getASIC()            { return getBitRange(_binaryData[2],20,23); }   // Error bit set if an error was logged in ASIC data.
        const uint8_t  getWIBErrors()       { return getBitRange(_binaryData[2],24,31); }   // Error bit set by the WIB.
        
        const uint32_t getCRC32()           { return getBitRange(_binaryData[3],0,31); }    // Complete checksum of frame.
        
        // Header/footer mutators.
        void setStreamID(uint8_t newStreamID)           { setBitRange(_binaryData[0],newStreamID,0,7); }
        void setResetCount(uint32_t newResetCount)      { setBitRange(_binaryData[0],newResetCount,8,31); }
        
        void setWIBTimestamp(uint32_t newWIBTimestamp)  { setBitRange(_binaryData[1],newWIBTimestamp,0,31); }
                                                                      
        void setCrateNo(uint8_t newCrateNo)             { setBitRange(_binaryData[2],newCrateNo,0,4); }
        void setSlotNo(uint8_t newSlotNo)               { setBitRange(_binaryData[2],newSlotNo,5,7); }
        void setFiberNo(uint8_t newFiberNo)             { setBitRange(_binaryData[2],newFiberNo,8,10); }
        void setCapture(uint8_t newCapture)             { setBitRange(_binaryData[2],newCapture,16,19); }
        void setASIC(uint8_t newASIC)                   { setBitRange(_binaryData[2],newASIC,20,23); }
        void setWIBErrors(uint8_t newWIBErrors)         { setBitRange(_binaryData[2],newWIBErrors,24,31); }
                                                                      
        void setCRC32(uint32_t newCRC32)                { setBitRange(_binaryData[3],newCRC32,0,31); }
    };
    
    struct COLDATA_block {
        uint32_t _binaryData[28];
        // COLDATA blocks accessors.
        const uint8_t  getChecksumA()       { return getBitRange(_binaryData[0],0,7); }     // Checksum of individual COLDATA blocks.
        const uint8_t  getChecksumB()       { return getBitRange(_binaryData[0],8,15); }    // Checksum of individual COLDATA blocks.
        const uint8_t  getS1Err()           { return getBitRange(_binaryData[0],16,19); }   // Indicate errors in capturing streams from COLDATA ASICS.
        const uint8_t  getS2Err()           { return getBitRange(_binaryData[0],20,23); }   // Indicate errors in capturing streams from COLDATA ASICS.
        
        const uint16_t getCOLDATA(int num)  { return getBitRange(_binaryData[1+num/2],16*(1-(num%2)),16*(2-(num%2))); } // Raw data.
        
        // COLDATA blocks mutators.
        void setChecksumA(uint8_t newChecksumA)       { setBitRange(_binaryData[0],newChecksumA,0,7); }
        void setChecksumB(uint8_t newChecksumB)       { setBitRange(_binaryData[0],newChecksumB,8,15); }
        void setS1Err(uint8_t newS1Err)               { setBitRange(_binaryData[0],newS1Err,16,19); }
        void setS2Err(uint8_t newS2Err)               { setBitRange(_binaryData[0],newS2Err,20,23); }
        
        void setCOLDATA(int num, uint16_t newCOLDATA) { setBitRange(_binaryData[1+num/2],newCOLDATA,16*(1-(num%2)),16*(2-(num%2))); }
    };
    
    class Frame {
        // Frame structure from Eric Hazen's preliminary WIB->FELIX frame format:
        // http://docs.dunescience.org/cgi-bin/RetrieveFile?docid=1701&filename=ProtoDUNE_to_FELIX.pdf&version=1
    private:
        WIB_header head;
        COLDATA_block block[4];
        uint32_t* _binaryData[116]; // Pointers to all data of a single frame for easy access.
    public:
        // Frame constructor links the binary data from the header and block structs.
        Frame() {
            for(int i=0; i<3; i++)
                _binaryData[i] = &head._binaryData[i];
            _binaryData[115] = &head._binaryData[3];
            
            for(int i=0; i<4*28; i++)
                _binaryData[3+(i/28)*28+i%28] = &block[i/28]._binaryData[i%28];
        }
        
        // Header/footer accessors.
        const uint8_t  getStreamID()        { return head.getStreamID(); }      // Data type (trigger, calibration, unbiased, etc.) to route data.
        const uint32_t getResetCount()      { return head.getResetCount(); }    // Increments by one every 2^16 frames.
        const uint32_t getWIBTimestamp()    { return head.getWIBTimestamp(); }  // Local timestamp.
        const uint8_t  getCrateNo()         { return head.getCrateNo(); }       // Crate number.
        const uint8_t  getSlotNo()          { return head.getSlotNo(); }        // Slot number.
        const uint8_t  getFiberNo()         { return head.getFiberNo(); }       // Fiber number.
        const uint8_t  getCapture()         { return head.getCapture(); }       // Error bit set if an error occurred during data capture.
        const uint8_t  getASIC()            { return head.getASIC(); }          // Error bit set if an error was logged in ASIC data.
        const uint8_t  getWIBErrors()       { return head.getWIBErrors(); }     // Error bit set by the WIB.
        const uint32_t getCRC32()           { return head.getCRC32(); }         // Complete checksum of frame.
        // COLDATA blocks accessors.
        const uint8_t  getChecksumA(int blockNum)           { return block[blockNum].getChecksumA(); }  // Checksum of individual COLDATA blocks.
        const uint8_t  getChecksumB(int blockNum)           { return block[blockNum].getChecksumB(); }  // Checksum of individual COLDATA blocks.
        const uint8_t  getS1Err(int blockNum)               { return block[blockNum].getS1Err(); }      // Indicate errors in capturing streams from COLDATA ASICS.
        const uint8_t  getS2Err(int blockNum)               { return block[blockNum].getS2Err(); }      // Indicate errors in capturing streams from COLDATA ASICS.
        const uint16_t getCOLDATA(int blockNum, int num)    { return block[blockNum].getCOLDATA(num); } // Raw data.
        // Struct accessors.
        const WIB_header getWIBHeader()                     { return head; }
        const COLDATA_block getCOLDATABlock(int blockNum)   { return block[blockNum]; }
        
        // Header/footer mutators.
        void setStreamID(uint8_t newStreamID)           { head.setStreamID(newStreamID); }
        void setResetCount(uint32_t newResetCount)      { head.setResetCount(newResetCount); }
        void setWIBTimestamp(uint32_t newWIBTimestamp)  { head.setWIBTimestamp(newWIBTimestamp); }
        void setCrateNo(uint8_t newCrateNo)             { head.setCrateNo(newCrateNo); }
        void setSlotNo(uint8_t newSlotNo)               { head.setSlotNo(newSlotNo); }
        void setFiberNo(uint8_t newFiberNo)             { head.setFiberNo(newFiberNo); }
        void setCapture(uint8_t newCapture)             { head.setCapture(newCapture); }
        void setASIC(uint8_t newASIC)                   { head.setASIC(newASIC); }
        void setWIBErrors(uint8_t newWIBErrors)         { head.setWIBErrors(newWIBErrors); }
        void setCRC32(uint32_t newCRC32)                { head.setCRC32(newCRC32); }
        // COLDATA blocks mutators.
        void setChecksumA(int blockNum, uint8_t newChecksumA)       { block[blockNum].setChecksumA(newChecksumA); }
        void setChecksumB(int blockNum, uint8_t newChecksumB)       { block[blockNum].setChecksumB(newChecksumB); }
        void setS1Err(int blockNum, uint8_t newS1Err)               { block[blockNum].setS1Err(newS1Err); }
        void setS2Err(int blockNum, uint8_t newS2Err)               { block[blockNum].setS2Err(newS2Err); }
        void setCOLDATA(int blockNum, int num, uint16_t newCOLDATA) { block[blockNum].setCOLDATA(num, newCOLDATA); }
        // Struct mutators.
        void setWIBHeader(WIB_header newWIBHeader)                          { head = newWIBHeader; }
        void setCOLDATABlock(int blockNum, COLDATA_block newCOLDATABlock)   { block[blockNum] = newCOLDATABlock; }
        
        bool load(std::string filename, int frameNum = 0);
        void load(std::ifstream& strm, int frameNum = 0);
        
        // Longitudinal redundancy check (8-bit).
        uint8_t checksum_A(unsigned int blockNum, uint8_t init = 0);
        // Modular checksum (8-bit).
        uint8_t checksum_B(unsigned int blockNum, uint8_t init = 0);
        // Cyclic redundancy check (32-bit).
        uint32_t CRC32(uint32_t padding = 0, uint32_t CRC32_Polynomial = CRC32_POLYNOMIAL);
        // Zlib's cyclic redundancy check (32-bit).
        uint32_t zCRC32(uint32_t padding = 0);
        
        // Overloaded and friended frame print functions.
        bool print(std::string filename, char opt = 'b');
        bool print(std::ofstream& strm, char opt = 'b');
        friend bool print(const Frame& frame, std::string filename, char opt, const int Nframes);
        friend bool print(const Frame& frame, std::ofstream& strm, char opt, const int Nframes);
    };
    
    // Function to check whether a frame corresponds to its checksums.
    const bool check(const std::string& filename);
    // Function to check frames within a single file.
    const bool checkSingleFile(const std::string& filename);
    
    // Functions to compress and decompress frames or sets of frames by a file name.
    const bool compressFile(const std::string& filename);
    const bool decompressFile(const std::string& filename);
    
    // Frame print functions.
    bool print(const Frame& frame, std::string filename, char opt = 'b', const int Nframes = 1);
    bool print(const Frame& frame, std::ofstream& strm, char opt = 'b', const int Nframes = 1);
    
    class FrameGen {
    private:
        // File data.
        std::string _path = "exampleframes/";
        std::string _prefix = "test";
        std::string _suffix = "";
        std::string _extension = ".frame";
        
        unsigned int _frameNo = 0; // Total number of frames generated by this generator.
        
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
        
        void fill();
        
    public:
        // Constructors/destructors.
        FrameGen(): _mt(_rd()), _randDouble(0.0, 1.0){}
        FrameGen(const std::string& prefix): _prefix(prefix), _mt(_rd()), _randDouble(0.0, 1.0){}
        FrameGen(const int maxNoise): _noiseAmplitude(maxNoise){}
        ~FrameGen(){}
        
        // Frame name accessors/modifiers.
        void setPath(std::string path)              { _path = path; }
        const std::string& getPath()                { return _path; }
        void setPrefix(std::string prefix)          { _prefix = prefix; }
        const std::string& getPrefix()              { return _prefix; }
        void setSuffix(std::string suffix)          { _suffix = suffix; }
        const std::string& getSuffix()              { return _suffix; }
        void setExtension(std::string extension)    { _extension = extension; }
        const std::string& getExtension()           { return _extension; }
        
        const std::string getFileName(unsigned long i)  { return _path + _prefix + std::to_string(i) + _suffix + _extension; }
        const std::string getFileName()                 { return _path + _prefix + _suffix + _extension; }
        
        // Noise parameter accessors/modifiers.
        void setPedestal(uint16_t pedestal)     { _noisePedestal = pedestal; }
        const uint16_t getPedestal()            { return _noisePedestal; }
        void setAmplitude(uint16_t amplitude)   { _noiseAmplitude = amplitude; }
        const uint16_t getAmplitude()           { return _noiseAmplitude; }
        
        // Main generator function: builds frames and calls the fill function.
        void generate(const unsigned long Nframes = 1, char opt = 'b');
        void generate(const std::string& newPrefix, const unsigned long Nframes = 1, char opt = 'b');
        
        // Generator function to create a certain number of frames and place them in the same file.
        void generateSingleFile(const unsigned long Nframes = 1, char opt = 'b');
        void generateSingleFile(const std::string& newPrefix, const unsigned long Nframes = 1, char opt = 'b');
        
        // Function that attempts to open a file by its name, trying variations with class parameters (_extension, _path, etc.).
        const bool openFile(std::ifstream& strm, const std::string& filename);
        
        // Overloaded and ranged check functions that absolutely require the FrameGen filename parameters.
        const bool check() { return framegen::check(_path+_prefix+"0"+_suffix+_extension); }
        const bool check(const unsigned int begin, const unsigned int end);
        const bool check(const unsigned int end) { return check(0,end); }
        const bool checkSingleFile() { return framegen::checkSingleFile(getFileName()); }
        
        // Overloaded compression/decompression functions.
        const bool compressFile()   { return framegen::compressFile(getFileName()); }
        const bool decompressFile() { return framegen::decompressFile(getFileName()); }
        
        // Overloaded frame print functions.
        bool print(char opt = 'b') { return framegen::print(_frame, getFileName(), opt); }
        bool print(std::string filename, char opt = 'b') { return framegen::print(_frame, filename, opt); }
        bool print(std::ofstream& strm, char opt = 'b') { return framegen::print(_frame, strm, opt); }
    };
    
} // namespace framegen

#endif /* FRAMEGEN_HPP_ */

