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

#define FRAME_LENGTH 117
#define CRC32_POLYNOMIAL 3988292384

namespace framegen {
    
  //  static uint64_t numberOfFramesGlobal = 0;
    
  // TO DO: Convert to uint64_t-compatible version.
  const uint32_t getBitRange(const uint32_t& word, int begin, int end) {
    if(begin==0 && end==31)
      return word;
    else
      return (word>>begin)&((1<<(end-begin+1))-1);
  }
    
  template <typename T>
  void setBitRange(uint32_t& word, const T& newValue, int begin, int end) {
    if(begin==0 && end==31) {
      word = newValue;
      return;
    }
    uint32_t mask = (1<<(end-begin+1))-1;
    word = (word&~(mask<<begin)) | ((newValue&mask)<<begin);
  }
    
    struct WIB_header {
        // Header/footer accessors.
        uint32_t _binaryData[5];
        const uint8_t getK28_5()    { return getBitRange(_binaryData[0],0,7); }     // K28.5 only for RCE, undefined for FELIX.
        const uint8_t getVersion()  { return getBitRange(_binaryData[0],8,12); }    // Version number.
        const uint8_t  getFiberNo() { return getBitRange(_binaryData[0],13,15); }   // Fiber number.
        const uint8_t  getCrateNo() { return getBitRange(_binaryData[0],16,20); }   // Crate number.
        const uint8_t  getSlotNo()  { return getBitRange(_binaryData[0],21,23); }   // Slot number.
        
        const uint16_t  getWIBErrors()  { return getBitRange(_binaryData[1],16,31); }   // Error bit set by the WIB. Details TBD.
        
        const uint8_t getZ()            { return getBitRange(_binaryData[3],31,31); }
        const uint64_t getTimestamp()   {
            return (uint64_t)getBitRange(_binaryData[2],0,31)
            | (uint64_t)getBitRange(_binaryData[3],0,15)<<31
            | (getZ()? 0: ((uint64_t)getBitRange(_binaryData[3],16,30)<<47));
        }    // Timestamp. 63 bit if Z==0, 48 bit if Z==1.
        const uint16_t getWIBCounter()  { return getZ()? getBitRange(_binaryData[3],16,30): 0; } // WIB Counter, only exists when Z==1.
        
        const uint32_t getCRC32()       { return getBitRange(_binaryData[4],0,31); }    // Complete checksum of frame.
        
        // Header/footer mutators.
        void setK28_5(uint8_t newK28_5)     { setBitRange(_binaryData[0],newK28_5,0,7); }
        void setVersion(uint8_t newVersion) { setBitRange(_binaryData[0],newVersion,8,12); }
        void setFiberNo(uint8_t newFiberNo) { setBitRange(_binaryData[0],newFiberNo,13,15); }
        void setCrateNo(uint8_t newCrateNo) { setBitRange(_binaryData[0],newCrateNo,16,20); }
        void setSlotNo(uint8_t newSlotNo)   { setBitRange(_binaryData[0],newSlotNo,21,23); }
        
        void setWIBErrors(uint16_t newWIBErrors)     { setBitRange(_binaryData[1],newWIBErrors,16,31); }
        
        void setZ(uint8_t newZ)             { setBitRange(_binaryData[3],newZ,31,31); }
        void setTimestamp(uint64_t newTimestamp)     {
            setBitRange(_binaryData[2],getBitRange(newTimestamp,0,31),0,31);
            setBitRange(_binaryData[3],getBitRange(newTimestamp>>31,0,15),0,15);
            if(getZ()==0) setBitRange(_binaryData[3],getBitRange(newTimestamp>>31,16,30),16,30);
        }
        void setWIBCounter(uint16_t newWIBCounter)    { if(getZ()==1) setBitRange(_binaryData[3],newWIBCounter,16,30); }
        
        void setCRC32(uint32_t newCRC32)         { setBitRange(_binaryData[4],newCRC32,0,31); }
    };
    
    struct COLDATA_block {
        uint32_t _binaryData[28];
        // COLDATA block accessors.
        const uint16_t  getChecksumA()  { return getBitRange(_binaryData[0],16,23) | getBitRange(_binaryData[1],16,23)<<8; } // Odd streams.
        const uint16_t  getChecksumB()  { return getBitRange(_binaryData[0],24,31) | getBitRange(_binaryData[1],24,31)<<8; } // Even streams.
        const uint16_t  getAErr()       { return getBitRange(_binaryData[2],16,23) | getBitRange(_binaryData[3],16,23)<<8; } // Odd streams.
        const uint16_t  getBErr()       { return getBitRange(_binaryData[2],24,31) | getBitRange(_binaryData[3],24,31)<<8; } // Even streams.
        const uint8_t   getSErr(unsigned int stream) { return getBitRange(_binaryData[stream/2],4*(stream%2),4*(stream%2+1)-1); }
        
        const uint16_t getCOLDATA(unsigned int stream, unsigned int channel)  { // WARNING: stream [0:7], channel [0:7].
            if(stream>7 || channel>7) {
                std::cout << "Error (getCOLDATA()): stream or channel out of [0:7] range." << std::endl;
                return 0;
            }
            // Channel 3 and 6 are broken up in the current design of COLDATA.
            switch(channel) {
                case 2:
                    return getBitRange(_binaryData[4+stream*3],24,31) | getBitRange(_binaryData[4+stream*3+1],0,3)<<8;
                case 5:
                    return getBitRange(_binaryData[4+stream*3+1],28,31) | getBitRange(_binaryData[4+stream*3+2],0,7)<<4;
                default:
                    return getBitRange(_binaryData[4+stream*3+channel*12/32],(channel*12)%32,(channel*12)%32);
            }
        }
        
        // COLDATA block mutators.
        void setChecksumA(uint16_t newChecksumA) {
            setBitRange(_binaryData[0],getBitRange(newChecksumA,0,7),16,23);
            setBitRange(_binaryData[1],getBitRange(newChecksumA,8,15),16,23);
        }
        void setChecksumB(uint16_t newChecksumB) {
            setBitRange(_binaryData[0],getBitRange(newChecksumB,0,7),24,31);
            setBitRange(_binaryData[1],getBitRange(newChecksumB,8,15),24,31);
        }
        void setAErr(uint8_t newAErr) {
            setBitRange(_binaryData[2],getBitRange(newAErr,0,7),16,23);
            setBitRange(_binaryData[3],getBitRange(newAErr,8,15),16,23);
        }
        void setBErr(uint8_t newBErr) {
            setBitRange(_binaryData[2],getBitRange(newBErr,0,7),24,31);
            setBitRange(_binaryData[3],getBitRange(newBErr,8,15),24,31);
        }
        void setSErr(unsigned int stream, uint8_t newSErr) { setBitRange(_binaryData[stream/2],newSErr,4*(stream%2),4*(stream%2+1)-1); }
        
        void setCOLDATA(unsigned int stream, unsigned int channel, uint16_t newCOLDATA) {
            if(stream>7 || channel>7) {
                std::cout << "Error (getCOLDATA()): stream or channel out of [0:7] range." << std::endl;
                return;
            }
            switch(channel) {
                case 2:
                    setBitRange(_binaryData[4+stream*3],getBitRange(newCOLDATA,0,7),24,31);
                    setBitRange(_binaryData[4+stream*3+1],getBitRange(newCOLDATA,8,11),0,3);
                    break;
                case 5:
                    setBitRange(_binaryData[4+stream*3+1],getBitRange(newCOLDATA,0,3),28,31);
                    setBitRange(_binaryData[4+stream*3+2],getBitRange(newCOLDATA,4,11),0,7);
                    break;
                default:
                    setBitRange(_binaryData[4+stream*3+channel*12/32],newCOLDATA,(channel*12)%32,((channel+1)*12)%32);
                    break;
            }
        }
    };
    
    class Frame {
        // Frame structure 1.0 from Daniel Gastler.
    private:
        WIB_header head;
        COLDATA_block block[4];
        uint32_t* _binaryData[FRAME_LENGTH]; // Pointers to all data of a single frame for easy access.
    public:
        // Frame constructor links the binary data from the header and block structs.
        Frame() {
            for(int i=0; i<4; i++)
                _binaryData[i] = &head._binaryData[i];
            _binaryData[FRAME_LENGTH-1] = &head._binaryData[4];
            
            for(int i=0; i<4*28; i++)
                _binaryData[4+(i/28)*28+i%28] = &block[i/28]._binaryData[i%28];
        }
        
        // Header/footer accessors.
        const uint8_t  getK28_5()        { return head.getK28_5(); }         // K28.5 only for RCE, undefined for FELIX.
        const uint8_t  getVersion()      { return head.getVersion(); }       // Version number.
        const uint8_t  getFiberNo()     { return head.getFiberNo(); }       // Fiber number.
        const uint8_t  getCrateNo()     { return head.getCrateNo(); }       // Crate number.
        const uint8_t  getSlotNo()      { return head.getSlotNo(); }        // Slot number.
        const uint16_t getWIBErrors()  { return head.getWIBErrors(); }     // Error bit set by the WIB. Details TBD.
        const uint8_t getZ()            { return head.getZ(); }             // Timestamp option.
        const uint64_t getTimestamp()   { return head.getTimestamp(); }     // Timestamp. 63 bit if Z==0, 48 bit if Z==1.
        const uint16_t getWIBCounter()  { return head.getWIBCounter(); }    // WIB Counter, only exists when Z==1.
        const uint32_t getCRC32()       { return head.getCRC32(); }         // Complete checksum of frame.
        // COLDATA block accessors.
        const uint16_t getChecksumA(unsigned int blockNum)   { return block[blockNum].getChecksumA(); } // Odd streams. (TODO)
        const uint16_t getChecksumB(unsigned int blockNum)   { return block[blockNum].getChecksumB(); } // Even streams. (TODO)
        const uint8_t  getAErr(unsigned int blockNum)        { return block[blockNum].getAErr(); }      // Odd streams. (TODO)
        const uint8_t  getBErr(unsigned int blockNum)        { return block[blockNum].getBErr(); }      // Even streams. (TODO)
        const uint8_t  getSErr(unsigned int blockNum, unsigned int stream)                          { return block[blockNum].getSErr(stream); }
        const uint16_t getCOLDATA(unsigned int blockNum, unsigned int stream, unsigned int channel) { return block[blockNum].getCOLDATA(stream, channel); }
        // Struct accessors.
        const WIB_header getWIBHeader() { return head; }
        const COLDATA_block getCOLDATABlock(unsigned int blockNum) { return block[blockNum]; }
        
        // Header/footer mutators.
        void setK28_5(uint8_t newK28_5)             { head.setK28_5(newK28_5); }
        void setVersion(uint8_t newVersion)         { head.setVersion(newVersion); }
        void setFiberNo(uint8_t newFiberNo)         { head.setFiberNo(newFiberNo); }
        void setCrateNo(uint8_t newCrateNo)         { head.setCrateNo(newCrateNo); }
        void setSlotNo(uint8_t newSlotNo)           { head.setSlotNo(newSlotNo); }
        void setWIBErrors(uint16_t newWIBErrors)    { head.setWIBErrors(newWIBErrors); }
        void setZ(uint8_t newZ)                     { head.setZ(newZ); }
        void setTimestamp(uint64_t newTimestamp)    { head.setTimestamp(newTimestamp); }
        void setWIBCounter(uint16_t newWIBCounter)  { head.setWIBCounter(newWIBCounter); }
        void setCRC32(uint32_t newCRC32)            { head.setCRC32(newCRC32); }
        // COLDATA block mutators.
        void setChecksumA(unsigned int blockNum, uint16_t newChecksumA) { block[blockNum].setChecksumA(newChecksumA); }
        void setChecksumB(unsigned int blockNum, uint16_t newChecksumB) { block[blockNum].setChecksumB(newChecksumB); }
        void setAErr(unsigned int blockNum, uint8_t newAErr)            { block[blockNum].setAErr(newAErr); }
        void setBErr(unsigned int blockNum, uint8_t newBErr)            { block[blockNum].setBErr(newBErr); }
        void setSErr(unsigned int blockNum, unsigned int stream, uint8_t newSErr)                               { block[blockNum].setSErr(stream, newSErr); }
        void setCOLDATA(unsigned int blockNum, unsigned int stream, unsigned int channel, uint16_t newCOLDATA)  { block[blockNum].setCOLDATA(stream, channel, newCOLDATA); }
        // Struct mutators.
        void setWIBHeader(WIB_header newWIBHeader) { head = newWIBHeader; }
        void setCOLDATABlock(unsigned int blockNum, COLDATA_block newCOLDATABlock) { block[blockNum] = newCOLDATABlock; }
        
        bool load(std::string filename, int frameNum = 0);
        void load(std::ifstream& strm, int frameNum = 0);
        
        // Utility functions.
        void resetChecksums();
        void clearReserved();
        
        // Longitudinal redundancy check (8-bit).
        uint16_t checksum_A(unsigned int blockNum, uint16_t init = 0);
        // Modular checksum (8-bit).
        uint16_t checksum_B(unsigned int blockNum, uint16_t init = 0);
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
        uint16_t _noisePedestal = 250; // Pedestal of the noise (0 - 2^10).
        uint16_t _noiseAmplitude = 10; // Amplitude of the noise (0 - 2^10).
        
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

