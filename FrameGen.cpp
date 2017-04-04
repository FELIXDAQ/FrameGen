//============================================================================
// Name        : FrameGen.cpp
// Author      : Milo Vermeulen
// Version     :
// Copyright   : Copyright (c) 2017 All rights reserved
// Description : FrameGen in C++, Ansi-style
//============================================================================

#include "FrameGen.hpp"

namespace framegen {
    
    //=======
    // Frame
    //=======
    bool Frame::load(std::string filename, int frameNum) {
        // Open file which contains frame to load and move pointer to the beginning of the frame.
        std::ifstream ifile(filename);
        if(!ifile) {
            std::cout << "Error (Frame::load): file " << filename << " could not be opened." << std::endl;
            return false;
        }
        ifile.seekg(0,std::ios_base::end);
        if(ifile.tellg()<(frameNum+1)*464) {
            std::cout << "Error (Frame::load): file " << filename << " contains fewer than " << frameNum+1 << " frames." << std::endl << "Be sure to start counting at 0." << std::endl;
            return false;
        }
        
        load(ifile, frameNum);
        ifile.close();
        
        return true;
    }
    
    void Frame::load(std::ifstream& strm, int frameNum) {
        strm.seekg(frameNum*464);
        for(int i=0; i<116; i++)
            *_binaryData[i] = (uint32_t)strm.get() | strm.get()<<8 | strm.get()<<16 | strm.get()<<24;
    }
    
    // Longitudinal redundancy check (8-bit).
    uint8_t Frame::checksum_A(unsigned int blockNum, uint8_t init) {
        if(blockNum>3) {
            std::cout << "Error: invalid block number passed to checksum_A(). (Valid range: 0-3.)" << std::endl;
            return 0;
        }
        uint8_t result = init;
        for(unsigned int i=4+blockNum*28; i<4+blockNum*28+27; i++) {
            // Divide 32-bit into 8-bit and XOR.
            result ^= getBitRange(*_binaryData[i],0,7);
            result ^= getBitRange(*_binaryData[i],8,15);
            result ^= getBitRange(*_binaryData[i],16,23);
            result ^= getBitRange(*_binaryData[i],24,31);
        }
        return result;
    }
    
    // Modular checksum (8-bit).
    uint8_t Frame::checksum_B(unsigned int blockNum, uint8_t init) {
        if(blockNum>3) {
            std::cout << "Error: invalid block number passed to checksum_B(). (Valid range: 0-3.)" << std::endl;
            return 0;
        }
        uint8_t result = init;
        for(unsigned int i=4+blockNum*28; i<4+blockNum*28+27; i++) {
            // Divide 32-bit into 8-bit and add.
            result += getBitRange(*_binaryData[i],0,7);
            result += getBitRange(*_binaryData[i],8,15);
            result += getBitRange(*_binaryData[i],16,23);
            result += getBitRange(*_binaryData[i],24,31);
        }
        return -result;
    }
    
    // Cyclic redundancy check (32-bit).
    uint32_t Frame::CRC32(uint32_t padding, uint32_t CRC32_Polynomial) {
        uint32_t shiftReg = *_binaryData[0]; // Shifting register.
        if(shiftReg&1)
            shiftReg ^= CRC32_Polynomial;
        // Shift through the data.
        for(int i=0; i<114*32; i++) { // The register shifts through 115 32-bit words and is 32 bits long.
            // Perform XOR on the shifting register if the leading bit is 1 and shift.
            if(shiftReg & 1) {
                shiftReg = shiftReg>>1 | (*_binaryData[i/32+1]>>(i%32)&1)<<31;
                shiftReg ^= CRC32_Polynomial;
            } else
                shiftReg = shiftReg>>1 | (*_binaryData[i/32+1]>>(i%32)&1)<<31;
        }
        
        return shiftReg^padding;
    }
    
    // Zlib's cyclic redundancy check (32-bit).
    uint32_t Frame::zCRC32(uint32_t padding) {
        uint32_t crc = crc32(0L, Z_NULL, 0);
        uint8_t* p;
        for(int i=0; i<115; i++) {
            p = (uint8_t*)_binaryData[i];
            for(int j=0; j<4; j++)
                crc = crc32(crc, p++, 1);
        }
        return crc^padding;
    }
    
    // Overloaded frame print functions.
    bool Frame::print(std::string filename, char opt) { return framegen::print(*this, filename, opt); }
    bool Frame::print(std::ofstream& strm, char opt) { return framegen::print(*this, strm, opt); }
    
    
    //==========
    // FrameGen
    //==========
    void FrameGen::fill() {
        // Header.
        _frame.setStreamID(rand()%8);
        _frame.setResetCount(numberOfFrames>>16);
        
        { // Small timestamp scope.
            using namespace std::chrono;
            static nanoseconds time = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch());
            _frame.setWIBTimestamp(duration_cast<nanoseconds>(time).count());
            time += nanoseconds(500);
        }
        
        _frame.setCrateNo(rand()%32);
        _frame.setSlotNo(rand()%8);
        _frame.setFiberNo(rand()%8);
        _frame.setCapture(_randDouble(_mt)<_errProb);
        _frame.setASIC(_randDouble(_mt)<_errProb);
        _frame.setWIBErrors(_randDouble(_mt)<_errProb);
        
        // Produce four COLDATA blocks. (Random numbers have 16 bits each.)
        std::binomial_distribution<int> _randInt(_noiseAmplitude*2, 0.5);
        int randnum = 0; // Temporary variable to make sure random values are positive.
        for(int i=0; i<4; i++) {
            // Build 27 32-bit words of COLDATA in sets of two 16-bit numbers.
            for(int j=0; j<27*2; j++) {
                do {
                    randnum = _randInt(_mt)-_noiseAmplitude+_noisePedestal;
                } while(randnum<0);
                _frame.setCOLDATA(i,j,randnum);
            }
            
            // Generate checksums and insert them.
            _frame.setChecksumA(i,_frame.checksum_A(i));
            _frame.setChecksumB(i,_frame.checksum_B(i));
            _frame.setS1Err(i,_randDouble(_mt)<_errProb);
            _frame.setS2Err(i,_randDouble(_mt)<_errProb);
        }
        
        // Final checksum over the entire frame.
        _frame.setCRC32(_frame.CRC32());
        
        // Increment the number of frames.
        numberOfFrames++;
    }
    
    // Main generator function: builds frames and calls the fill function.
    void FrameGen::generate(const unsigned long Nframes, char opt) {
        if(_path == "")
            std::cout << "Generating frames." << std::endl;
        else
            std::cout << "Generating frames at path " << _path << "." << std::endl;
        for(unsigned long i=0; i<Nframes; i++) {
            // Open frame, fill it and close it.
            std::ofstream ofile(getFileName(i));
            if(!ofile) {
                std::cout << "Error (generate()): " << getFileName(i) << " could not be opened." << std::endl;
                std::cout << "\t(You will have to create the path " << _path << " if you haven't already.)" << std::endl;
                return;
            }
            fill();
            _frame.print(ofile, opt);
            ofile.close();
            
            // Keep track of progress.
            if((i*100)%Nframes==0)
                std::cout << i*100/Nframes << "%\r" << std::flush;
            _frameNo++;
        }
        std::cout << "    \tDone." << std::endl;
    }
    
    // Overloaded generate function to handle new prefixes.
    void FrameGen::generate(const std::string& newPrefix, const unsigned long Nframes, char opt) {
        _prefix = newPrefix;
        generate(Nframes, opt);
    }
    
    // Generator function to create a certain number of frames and place them in the same file.
    void FrameGen::generateSingleFile(const unsigned long Nframes, char opt) {
        if(_path == "")
            std::cout << "Generating frames." << std::endl;
        else
            std::cout << "Generating frames at path " << _path << "." << std::endl;
        // Open frame, fill it and close it.
        std::string filename = _path+_prefix+_suffix+_extension;
        std::ofstream ofile(filename);
        if(!ofile) {
            std::cout << "Error (generate()): " << filename << " could not be opened." << std::endl;
            return;
        }
        for(unsigned long i=0; i<Nframes; i++) {
            fill();
            framegen::print(_frame, ofile, opt, Nframes);
            
            // Keep track of progress.
            if((i*100)%Nframes==0)
                std::cout << i*100/Nframes << "%\r" << std::flush;
            _frameNo++;
        }
        ofile.close();
        std::cout << "    \tDone." << std::endl;
    }
    
    // Overloaded generate function to handle new prefixes.
    void FrameGen::generateSingleFile(const std::string& newPrefix, const unsigned long Nframes, char opt) {
        _prefix = newPrefix;
        generateSingleFile(Nframes, opt);
    }
    
    // Function that attempts to open a file by its name, trying variations with class parameters (_extension, _path, etc.).
    const bool FrameGen::openFile(std::ifstream& strm, const std::string& filename){
        std::string possibleName[5];
        possibleName[0] = filename;
        possibleName[1] = filename+_extension;
        possibleName[2] = _path+filename+_extension;
        possibleName[3] = filename+_suffix+_extension;
        possibleName[4] = _path+filename+_suffix+_extension;
        
        for(int i=0; !strm && i<5; i++) {
            strm.open(possibleName[i], std::ios::binary);
            if(strm) break;
        }
        
        return strm? true: false;
    }
    
    // Overloaded check functions to handle a range of files.
    const bool FrameGen::check(const unsigned int begin, const unsigned int end) {
        if(_path == "")
            std::cout << "Checking frames." << std::endl;
        else
            std::cout << "Checking frames at path " << _path << "." << std::endl;
        for(unsigned int i=begin; i<end; i++)
            if(!framegen::check(getFileName(i)))
                return false;
        return true;
    }
    
    
    //======================
    // Classless functions.
    //======================
    // Function to check whether a frame corresponds to its checksums and whether any of its error bits are set.
    const bool check(const std::string& filename) {
        Frame frame;
        if(!frame.load(filename)) {
            std::cout << "Error (framegen::check()): file " << filename << " could not be opened." << std::endl;
            return false;
        }
        
        // Check checksums.
        for(int i=0; i<4; i++) {
            if(frame.checksum_A(i, frame.getChecksumA(i)))
                std::cout << "Frame " << filename << ", COLDATA block " << i+1 << "/4 contains an error in checksum A." << std::endl;
            if(frame.checksum_B(i, frame.getChecksumB(i)))
                std::cout << "Frame " << filename << ", COLDATA block " << i+1 << "/4 contains an error in checksum B." << std::endl;
        }
        if(frame.CRC32(frame.getCRC32())) {
            std::cout << "Frame " << filename << " failed its cyclic redundancy check." << std::endl;
            return false;
        }
        
        // Check errors and produce a warning.
        if(frame.getCapture()) // Capture
            std::cout << "Warning: Capture error bit set in frame " << filename << "." << std::endl;
        if(frame.getASIC()) // ASIC
            std::cout << "Warning: ASIC error bit set in frame " << filename << "." << std::endl;
        if(frame.getWIBErrors()) // WIB_Errors
            std::cout << "Warning: WIB error bit set in frame " << filename << "." << std::endl;
        for(int i=0; i<4; i++) {
            if(frame.getS1Err(i)) // S1
                std::cout << "Warning: S1 error bit set in frame " << filename << ", block " << i+1 << "/4." << std::endl;
            if(frame.getS2Err(i)) // S2
                std::cout << "Warning: S2 error bit set in frame " << filename << ", block " << i+1 << "/4." << std::endl;
        }
        
        return true;
    }
    
    // Function to check files within a single file.
    const bool checkSingleFile(const std::string& filename) {
        // Open file.
        std::ifstream ifile(filename, std::ios::binary);
        if(!ifile) {
            std::cout << "Error (check()): could not open file " << filename << "." << std::endl;
            return false;
        }
        std::cout << "Checking frames." << std::endl;
        
        // Get number of frames and check whether this is an integer.
        ifile.seekg(0,ifile.end);
        if(ifile.tellg()%464) {
            std::cout << "Error: file " << filename << " contains unreadable frames." << std::endl;
            return false;
        }
        int numberOfFrames = ifile.tellg()/464;
        ifile.seekg(0,ifile.beg);
        
        Frame frame;
        
        bool result = true;
        
        for(int j=0; j<numberOfFrames; j++) {
            // Load data from file.
            frame.load(ifile,j);
            
            // Check checksums.
            for(int i=0; i<4; i++) {
                if(frame.checksum_A(i, frame.getChecksumA(i)))
                    std::cout << "Frame " << j << " of file " << filename << ", COLDATA block " << i+1 << "/4 contains an error in checksum A." << std::endl;
                if(frame.checksum_B(i, frame.getChecksumB(i)))
                    std::cout << "Frame " << j << " of file " << filename << ", COLDATA block " << i+1 << "/4 contains an error in checksum B." << std::endl;
            }
            if(frame.CRC32(frame.getCRC32())) {
                std::cout << "Frame " << j << " of file " << filename << " failed its cyclic redundancy check." << std::endl;
                return false;
            }
            
            // Check errors and produce a warning.
            if(frame.getCapture()) // Capture
                std::cout << "Warning: Capture error bit set in frame " << j << " of file " << filename << "." << std::endl;
            if(frame.getASIC()) // ASIC
                std::cout << "Warning: ASIC error bit set in frame " << j << " of file " << filename << "." << std::endl;
            if(frame.getWIBErrors()) // WIB_Errors
                std::cout << "Warning: WIB error bit set in frame " << j << " of file " << filename << "." << std::endl;
            for(int i=0; i<4; i++) {
                if(frame.getS1Err(i)) // S1
                    std::cout << "Warning: S1 error bit set in frame " << j << " of file " << filename << ", block " << i+1 << "/4." << std::endl;
                if(frame.getS2Err(i)) // S2
                    std::cout << "Warning: S2 error bit set in frame " << j << " of file " << filename << ", block " << i+1 << "/4." << std::endl;
            }
        }
        
        ifile.close();
        return result;
    }
    
    // Frame print functions.
    bool print(const Frame& frame, std::string filename, char opt, const int Nframes) {
        std::ofstream oframe(filename);
        if(!oframe) {
            std::cout << "Error (Frame::print): file " << filename << " could not be accessed." << std::endl;
            return false;
        }
        bool result = framegen::print(frame, oframe, opt, Nframes);
        oframe.close();
        return result;
    }
    
    bool print(const Frame& frame, std::ofstream& strm, char opt, const int Nframes) {
        if(!strm)
            return false;
        // b = binary, h = hexadecimal, o = octal, d = decimal, f = header file
        switch(opt) {
            case 'b':
                for(int i=0; i<116; i++)
                    strm << (char)(*frame._binaryData[i]) << (char)(*frame._binaryData[i]>>8) << (char)(*frame._binaryData[i]>>16) << (char)(*frame._binaryData[i]>>24);
                break;
            case 'h':
                for(int i=0; i<116; i++)
                    strm << std::hex << std::setfill('0') << "0x" << std::setw(8) << *frame._binaryData[i] << std::endl;
                break;
            case 'o':
                for(int i=0; i<116; i++)
                    strm << std::oct << std::setfill('0') << "0" << std::setw(11) << *frame._binaryData[i] << std::endl;
                break;
            case 'd':
                for(int i=0; i<116; i++)
                    strm << std::setfill('0') << std::setw(10) << *frame._binaryData[i] << std::endl;
                break;
            case 'f':
                // Add a header if this is the first frame. Otherwise adjust the cursor accordingly.
                if(strm.tellp()==0)
                    strm << "const uint32_t PROTODUNE_FRAMESIZE = 116*4;\n"
                    << "const uint32_t PROTODUNE_FRAMENUM = " << Nframes << ";\n\n"
                    << "uint32_t PROTODUNE_DATA[] = {";
                else {
                    strm.seekp(-3, std::ios::end);
                    strm << ",";
                }
                // Enter data.
                strm << std::endl << std::hex << std::setfill('0') << "    0x" << std::setw(8) << *frame._binaryData[0];
                for(int i=1; i<116; i++) {
                    strm << "," << std::endl << std::setfill('0') << "    0x" << std::setw(8) << *frame._binaryData[i];
                }
                strm << std::endl << "};";
                break;
            default:
                std::cout << "Error (Frame::print()): unknown print option '" << opt << "'." << std::endl;
                std::cout << "Valid print options: b = binary, h = hexadecimal, o = octal, d = decimal, f = header file." << std::endl;
                return false;
        }
        return true;
    }
    
    
    //============================
    // Compression/decompression.
    //============================
    // Function to compress frames or sets of frames.
    const bool compressFile(const std::string& filename) {
        // Open original file.
        std::ifstream ifcomp(filename, std::ios::binary);
        if(!ifcomp) {
            std::cout << "Error (compress()): file " << filename << " could not be openend." << std::endl;
            return false;
        }
        
        // Get file length and create buffer (to be changed to shifting window).
        ifcomp.seekg(0,ifcomp.end);
        unsigned long iflength = ifcomp.tellg();
        ifcomp.seekg(0,ifcomp.beg);
        
        Bytef* ifbuff = new Bytef[iflength];
        ifcomp.read((char*)ifbuff,iflength);
        
        // Create file to output compressed data into.
        std::ofstream ofcomp(filename+".comp", std::ios::binary);
        if(!ofcomp) {
            std::cout << "Error (compress()): file " << filename+".comp" << " could not be created." << std::endl;
            return false;
        }
        
        // Create buffer for compressed file and perform the compression (with the compression function from zlib).
        unsigned long oflength = iflength*1.1 + 12; // The new buffer needs to be longer than the original only until compression is complete.
        Bytef* ofbuff = new Bytef[oflength];
        switch(::compress(ofbuff, &oflength, ifbuff, iflength)) {
            case Z_OK:          break;
            case Z_MEM_ERROR:   std::cout << "Error (compress()): out of memory." << std::endl;                   return false;
            case Z_BUF_ERROR:   std::cout << "Error (compress()): output buffer not large enough." << std::endl;  return false;
            case Z_DATA_ERROR:  std::cout << "Error (compress()): the data was corrupted." << std::endl;          return false;
            default:            std::cout << "Error (compress()): unknown error code." << std::endl;              return false;
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
    
    const bool decompressFile(const std::string& filename) {
        // Check whether the filename has the right extension.
        bool compExt = true;
        // Compare filename extension to ".comp".
        if(strcmp(filename.substr(filename.rfind("."),filename.length()).c_str(), ".comp")) {
            std::cout << "Warning: the file " << filename << " does not have the default \".comp\" extension." << std::endl;
            compExt = false;
        }
        // Open original file.
        std::ifstream ifdecomp(filename, std::ios::binary);
        if(!ifdecomp) {
            std::cout << "Error (decompress()): file " << filename << " could not be opened." << std::endl;
            return false;
        }
        
        // Get file length and create buffer (to be changed to shifting window).
        ifdecomp.seekg(0,ifdecomp.end);
        unsigned long iflength = ifdecomp.tellg();
        ifdecomp.seekg(0,ifdecomp.beg);
        
        Bytef* ifbuff = new Bytef[iflength];
        ifdecomp.read((char*)ifbuff,iflength);
        
        // Create file to output compressed data into.
        std::ofstream ofdecomp(compExt? filename.substr(0,filename.length()-5): filename);
        if(!ofdecomp) {
            std::cout << "Error (decompress()): file " << (compExt? filename.substr(0,filename.length()-5): filename) << " could not be created." << std::endl;
            return false;
        }
        
        // Create buffer for decompressed file and perform the decompression (with the decompression function from zlib).
        unsigned long oflength = iflength*30; // Just to be safe.
        Bytef* ofbuff = new Bytef[oflength];
        switch(::uncompress(ofbuff, &oflength, ifbuff, iflength)) {
            case Z_OK:          break;
            case Z_MEM_ERROR:   std::cout << "Error (decompress()): out of memory." << std::endl;                     return false;
            case Z_BUF_ERROR:   std::cout << "Error (decompress()): output buffer not large enough." << std::endl;    return false;
            case Z_DATA_ERROR:  std::cout << "Error (decompress()): the data was corrupted." << std::endl;            return false;
            default:            std::cout << "Error (decompress()): unknown error code." << std::endl;                return false;
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
    
} // namespace framegen

