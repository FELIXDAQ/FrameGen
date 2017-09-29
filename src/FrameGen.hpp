//==========================================================================
// Name        : FrameGen.hpp
// Author      : Milo Vermeulen
// Version     :
// Copyright   : Copyright (c) 2017 All rights reserved
// Description : FrameGen in C++, Ansi-style
//============================================================================

#ifndef FRAMEGEN_HPP_
#define FRAMEGEN_HPP_

#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "zlib.h"

#define CRC32_POLYNOMIAL 3988292384

namespace framegen {
typedef uint32_t word_t;
typedef uint16_t adc_t;

// Constants for general use.
static const unsigned num_frame_hdr_words = 4;
static const unsigned num_COLDATA_hdr_words = 4;
static const unsigned num_frame_words = 117;
static const unsigned num_frame_bytes = num_frame_words * 4;
static const unsigned num_COLDATA_words = 28;

static const unsigned num_ch_per_frame = 256;
static const unsigned num_ch_per_block = 64;
static const unsigned num_stream_per_block = 8;
static const unsigned num_ch_per_stream = 8;

const uint32_t getBitRange(const uint32_t& word, int begin, int end) {
  if (begin == 0 && end == 31)
    return word;
  else
    return (word >> begin) & ((1 << (end - begin + 1)) - 1);
};

template <typename W, typename T>
void setBitRange(W& word, const T& newValue, int begin, int end) {
  if (begin == 0 && end == 31) {
    word = newValue;
    return;
  }
  uint32_t mask = (1 << (end - begin + 1)) - 1;
  word = (word & ~(mask << begin)) | ((newValue & mask) << begin);
};

// ==================================================================
// Substructures of WIB frames according to the current known format.
// ==================================================================
struct WIBHeader {
  word_t sof : 8, version : 5, fiber_no : 3, slot_no : 5, crate_no : 3,
      /*reserved*/ : 8;
  word_t mm : 1, oos : 1, /*reserved*/ : 14, wib_errors : 16;
  word_t timestamp_1 /*:32*/;
  word_t timestamp_2 : 16, wib_counter : 15, z : 1;

  uint64_t timestamp() const {
    uint64_t final_ts = (uint64_t)timestamp_1 | (uint64_t)timestamp_2 << 32;
    return bool(z) ? final_ts : final_ts | (uint64_t)wib_counter << 48;
  }

  void set_timestamp(const uint64_t& newTimestamp) {
    timestamp_1 = newTimestamp;
    timestamp_2 = newTimestamp >> 32;
    if (!z) {
      wib_counter = newTimestamp >> 48;
    }
  }

  uint16_t WIB_counter() const { return z ? wib_counter : 0; }

  // Print functions for debugging.
  void print() const {
    std::cout << "SOF:" << unsigned(sof) << " version:" << unsigned(version)
              << " fiber:" << unsigned(fiber_no)
              << " slot:" << unsigned(slot_no)
              << " crate:" << unsigned(crate_no) << " mm:" << unsigned(mm)
              << " oos:" << unsigned(oos)
              << " wib_errors:" << unsigned(wib_errors)
              << " timestamp: " << timestamp() << '\n';
  }
  void printHex() const {
    std::cout << std::hex << "SOF:" << sof << " version:" << version
              << " fiber:" << fiber_no << " slot:" << slot_no
              << " crate:" << crate_no << " mm:" << mm << " oos:" << oos
              << " wib_errors:" << wib_errors << " timestamp: " << timestamp()
              << std::dec << '\n';
  }

  void printBits() const {
    std::cout << "SOF:" << std::bitset<8>(sof)
              << " version:" << std::bitset<5>(version)
              << " fiber:" << std::bitset<3>(fiber_no)
              << " slot:" << std::bitset<5>(slot_no)
              << " crate:" << std::bitset<3>(crate_no) << " mm:" << bool(mm)
              << " oos:" << bool(oos)
              << " wib_errors:" << std::bitset<16>(wib_errors)
              << " timestamp: " << timestamp() << '\n'
              << " Z: " << z << '\n';
  }
};

struct ColdataHeader {
  word_t s1_error : 4, s2_error : 4, /*reserved*/ : 8, checksum_a_1 : 8,
      checksum_b_1 : 8;
  word_t checksum_a_2 : 8, checksum_b_2 : 8, coldata_convert_count : 16;
  word_t error_register : 16, /*reserved*/ : 16;
  word_t hdr;

  uint16_t checksum_a() const {
    return (uint16_t)checksum_a_1 | ((uint16_t)checksum_a_2) << 8;
  }
  void set_checksum_a(const uint16_t& newChecksum) {
    checksum_a_1 = newChecksum;
    checksum_a_2 = newChecksum >> 8;
  }
  uint16_t checksum_b() const {
    return (uint16_t)checksum_b_1 | ((uint16_t)checksum_b_2) << 8;
  }
  void set_checksum_b(const uint16_t& newChecksum) {
    checksum_b_1 = newChecksum;
    checksum_b_2 = newChecksum >> 8;
  }
  uint8_t HDR(const uint8_t& hdr_num) const {
    uint8_t mask = (1 << 4) - 1;
    return (hdr >> ((hdr_num % 8) * 4)) & mask;
  }
  void set_HDR(const uint8_t& hdr_num, const uint8_t& new_hdr) {
    uint32_t mask = (1 << 4) - 1;
    const unsigned bit_shift = (hdr_num % 8) * 4;
    uint32_t antimask = ~(mask << bit_shift);
    hdr = (hdr & antimask) | (new_hdr & mask) << bit_shift;
  }
};

struct ColdataBlock {
  ColdataHeader head;
  word_t adcs[24];

  adc_t channel(const uint8_t& adc, const uint8_t& ch) const {
    // All channel values are split in a first and second part. Because two
    // streams are put side-by-side, the effective word size per stream is 16
    // bits.
    uint8_t first_word = (adc / 2) * 6 + 12 * ch / 16;
    uint8_t first_offset = (12 * ch) % 16;

    // The split is at 8 bits for even channels and at 4 for odd ones.
    uint8_t split = 4 * (2 - ch % 2);

    uint8_t second_word = first_word + (first_offset + split) / 16;
    uint8_t second_offset = (first_offset + split) % 16;

    // Move offsets 8-15 to 16-23.
    first_offset += (first_offset / 8) * 8;
    second_offset += (second_offset / 8) * 8;

    // Left-shift odd streams by 8 bits.
    first_offset += (adc % 2) * 8;
    second_offset += (adc % 2) * 8;

    return getBitRange(adcs[first_word], first_offset,
                       first_offset + split - 1) |
           getBitRange(adcs[second_word], second_offset,
                       second_offset + 12 - split - 1)
               << split;
  }

  void set_channel(const uint8_t& adc, const uint8_t& ch,
                   const uint16_t& new_channel) {
    // Copied from the channel() function.
    uint8_t first_word = (adc / 2) * 6 + 12 * ch / 16;
    uint8_t first_offset = (12 * ch) % 16;

    // The split is at 8 bits for even channels and at 4 for odd ones.
    uint8_t split = 4 * (2 - ch % 2);

    uint8_t second_word = first_word + (first_offset + split) / 16;
    uint8_t second_offset = (first_offset + split) % 16;

    // Move offsets 8-15 to 16-23.
    first_offset += (first_offset / 8) * 8;
    second_offset += (second_offset / 8) * 8;

    // Left-shift odd streams by 8 bits.
    first_offset += (adc % 2) * 8;
    second_offset += (adc % 2) * 8;

    setBitRange(adcs[first_word], new_channel, first_offset,
                first_offset + split - 1);
    setBitRange(adcs[second_word], new_channel >> split, second_offset,
                second_offset + 12 - split - 1);
  }

  void printADCs() const {
    std::cout << "\t\t0\t1\t2\t3\t4\t5\t6\t7\n";
    for (int i = 0; i < 8; i++) {
      std::cout << "Stream " << i << ":\t";
      for (int j = 0; j < 8; j++) {
        std::cout << std::hex << channel(i, j) << '\t';
      }
      std::cout << '\n';
    }
  }
};

struct WIBFrame {
  WIBHeader head;
  ColdataBlock block[4];
  word_t CRC32;
};

// ==================================================================
// The main Frame class used to accept and give access to WIB frames.
// ==================================================================
class Frame {
  // Frame structure 1.0 from Daniel Gastler.
 private:
  word_t _binaryData[num_frame_words];
  WIBFrame* _frame = reinterpret_cast<WIBFrame*>(_binaryData);

 public:
  adc_t channel(uint8_t block_num, uint8_t adc, uint8_t ch) const {
    return _frame->block[block_num].channel(adc, ch);
  }
  adc_t channel(uint8_t ch) const {
    uint8_t block_num = ch / num_ch_per_block;
    uint8_t adc_num = ch / num_ch_per_stream;
    uint8_t ch_num = ch % num_ch_per_stream;

    return channel(block_num, adc_num, ch_num);
  }

  // WIB header accessors.
  uint8_t sof() { return _frame->head.sof; }
  uint8_t version() const { return _frame->head.version; }
  uint8_t fiber_no() const { return _frame->head.fiber_no; }
  uint8_t slot_no() const { return _frame->head.slot_no; }
  uint8_t crate_no() const { return _frame->head.crate_no; }
  uint8_t mm() const { return _frame->head.mm; }
  uint8_t oos() const { return _frame->head.oos; }
  uint16_t wib_errors() const { return _frame->head.wib_errors; }
  uint64_t timestamp() const { return _frame->head.timestamp(); }
  uint16_t wib_counter() const { return _frame->head.wib_counter; }
  uint8_t z() const { return _frame->head.z; }
  // WIB header modifiers.
  void set_sof(const uint8_t& newSof) { _frame->head.sof = newSof; }
  void set_version(const uint8_t& newVersion) {
    _frame->head.version = newVersion;
  }
  void set_fiber_no(const uint8_t& newFiber_no) {
    _frame->head.fiber_no = newFiber_no;
  }
  void set_slot_no(const uint8_t& newSlot_no) {
    _frame->head.slot_no = newSlot_no;
  }
  void set_crate_no(const uint8_t& newCrate_no) {
    _frame->head.crate_no = newCrate_no;
  }
  void set_mm(const uint8_t& newMm) { _frame->head.mm = newMm; }
  void set_oos(const uint8_t& newOos) { _frame->head.oos = newOos; }
  void set_wib_errors(const uint16_t& newWib_errors) {
    _frame->head.wib_errors = newWib_errors;
  }
  void set_timestamp(const uint64_t& newTimestamp) {
    _frame->head.set_timestamp(newTimestamp);
  }
  void set_wib_counter(const uint16_t& newWib_counter) {
    _frame->head.wib_counter = newWib_counter;
  }
  void set_z(const uint8_t& newZ) { _frame->head.z = newZ; }

  // Coldata block accessors.
  uint8_t s1_error(const uint8_t& block_num) const {
    return _frame->block[block_num].head.s1_error;
  }
  uint8_t s2_error(const uint8_t& block_num) const {
    return _frame->block[block_num].head.s2_error;
  }
  uint16_t checksum_a(const uint8_t& block_num) const {
    return _frame->block[block_num].head.checksum_a();
  }
  uint16_t checksum_b(const uint8_t& block_num) const {
    return _frame->block[block_num].head.checksum_b();
  }
  uint16_t coldata_convert_count(const uint8_t& block_num) const {
    return _frame->block[block_num].head.coldata_convert_count;
  }
  uint16_t error_register(const uint8_t& block_num) const {
    return _frame->block[block_num].head.error_register;
  }
  uint8_t HDR(const uint8_t& block_num, const uint8_t& HDR_num) const {
    return _frame->block[block_num].head.HDR(HDR_num);
  }
  uint16_t channel(const uint8_t& block_num, const uint8_t& adc,
                   const uint8_t& ch) {
    return _frame->block[block_num].channel(adc, ch);
  }
  uint16_t channel(const uint8_t& ch) {
    return channel(ch / num_ch_per_block,
                   (ch % num_ch_per_block) / num_ch_per_stream,
                   ch % num_ch_per_stream);
  }
  // Coldata block modifiers.
  void set_s1_error(const uint8_t& block_num, const uint8_t& new_s1_error) {
    _frame->block[block_num].head.s1_error = new_s1_error;
  }
  void set_s2_error(const uint8_t& block_num, const uint8_t& new_s2_error) {
    _frame->block[block_num].head.s2_error = new_s2_error;
  }
  void set_checksum_a(const uint8_t& block_num,
                      const uint16_t& new_checksum_a) {
    _frame->block[block_num].head.set_checksum_a(new_checksum_a);
  }
  void set_checksum_b(const uint8_t& block_num,
                      const uint16_t& new_checksum_b) {
    _frame->block[block_num].head.set_checksum_b(new_checksum_b);
  }
  void set_coldata_convert_count(const uint8_t& block_num,
                                 const uint16_t& new_coldata_convert_count) {
    _frame->block[block_num].head.coldata_convert_count =
        new_coldata_convert_count;
  }
  void set_error_register(const uint8_t& block_num,
                          const uint16_t& new_error_register) {
    _frame->block[block_num].head.error_register = new_error_register;
  }
  void set_HDR(const uint8_t& block_num, const uint8_t& HDR_num,
               const uint16_t& new_hdr) {
    _frame->block[block_num].head.set_HDR(HDR_num, new_hdr);
  }
  void set_channel(const uint8_t& block_num, const uint8_t& adc,
                   const uint8_t& ch, const uint16_t& new_channel) {
    _frame->block[block_num].set_channel(adc, ch, new_channel);
  }
  void set_channel(const uint8_t& ch,
                   const uint16_t& new_channel) {
    set_channel(ch / num_ch_per_block,
                (ch % num_ch_per_block) / num_ch_per_stream,
                ch % num_ch_per_stream, new_channel);
  }

  uint32_t CRC32() { return _frame->CRC32; }
  void set_CRC32(uint32_t newCRC32) { _frame->CRC32 = newCRC32; }

  void print() const {
    _frame->head.printHex();
    for (unsigned i = 0; i < 4; ++i) {
      std::cout << "Coldata block " << i << ":\n";
      //_frame->block[i].head.printHex();
      _frame->block[i].printADCs();
    }
  }

  // Struct mutators.
  void setWIBHeader(WIBHeader newWIBHeader) { _frame->head = newWIBHeader; }
  void setColdataBlock(unsigned int blockNum, ColdataBlock newColdataBlock) {
    _frame->block[blockNum] = newColdataBlock;
  }

  bool load(std::string filename, int frameNum = 0);
  void load(std::ifstream& strm, int frameNum = 0);
  void load(uint8_t* begin);

  // Utility functions.
  void resetChecksums();
  void clearReserved();

  // Longitudinal redundancy check (8-bit).
  uint16_t calculate_checksum_a(unsigned int blockNum, uint16_t init = 0);
  // Modular checksum (8-bit).
  uint16_t calculate_checksum_b(unsigned int blockNum, uint16_t init = 0);
  // Cyclic redundancy check (32-bit).
  uint32_t calculate_CRC32(uint32_t padding = 0,
                           uint32_t CRC32_Polynomial = CRC32_POLYNOMIAL);
  // Zlib's cyclic redundancy check (32-bit).
  uint32_t calculate_zCRC32(uint32_t padding = 0);

  // Overloaded and friended frame print functions.
  bool print(std::string filename, char opt = 'b');
  bool print(std::ofstream& strm, char opt = 'b');
  friend bool print(const Frame& frame, std::string filename, char opt,
                    const int Nframes);
  friend bool print(const Frame& frame, std::ofstream& strm, char opt,
                    const int Nframes);
}; // class Frame

// Function to check whether a frame corresponds to its checksums.
const bool check(const std::string& filename);
// Function to check frames within a single file.
const bool checkSingleFile(const std::string& filename);

// Functions to compress and decompress frames or sets of frames by a file name.
const bool compressFile(const std::string& filename);
const bool decompressFile(const std::string& filename);

// Frame print functions.
bool print(const Frame& frame, std::string filename, char opt = 'b',
           const int Nframes = 1);
bool print(const Frame& frame, std::ofstream& strm, char opt = 'b',
           const int Nframes = 1);

// =============================================================
// A generator class to generate frames in an automated fashion.
// =============================================================
class FrameGen {
 private:
  // File data.
  std::string _path = "exampleframes/";
  std::string _prefix = "test";
  std::string _suffix = "";
  std::string _extension = ".frame";

  unsigned int _frameNo =
      0;  // Total number of frames generated by this generator.

  // Frame contents.
  Frame _frame;

  // Noise data.
  double _errProb = 0.00001;      // Chance for any error bit to be set.
  uint16_t _noisePedestal = 250;  // Pedestal of the noise (0 - 2^10).
  uint16_t _noiseAmplitude = 10;  // Amplitude of the noise (0 - 2^10).

  // Random double generator to set error bits and generate noise.
  std::random_device _rd;
  std::mt19937 _mt;
  std::uniform_real_distribution<double> _randDouble;

  void fill();

 public:
  // Constructors/destructors.
  FrameGen() : _mt(_rd()), _randDouble(0.0, 1.0) {}
  FrameGen(const std::string& prefix)
      : _prefix(prefix), _mt(_rd()), _randDouble(0.0, 1.0) {}
  FrameGen(const int maxNoise) : _noiseAmplitude(maxNoise) {}
  ~FrameGen() {}

  // Frame name accessors/modifiers.
  void setPath(std::string path) { _path = path; }
  const std::string& getPath() { return _path; }
  void setPrefix(std::string prefix) { _prefix = prefix; }
  const std::string& getPrefix() { return _prefix; }
  void setSuffix(std::string suffix) { _suffix = suffix; }
  const std::string& getSuffix() { return _suffix; }
  void setExtension(std::string extension) { _extension = extension; }
  const std::string& getExtension() { return _extension; }

  const std::string getFileName(unsigned long i) {
    return _path + _prefix + std::to_string(i) + _suffix + _extension;
  }
  const std::string getFileName() {
    return _path + _prefix + _suffix + _extension;
  }

  // Noise parameter accessors/modifiers.
  void setPedestal(uint16_t pedestal) { _noisePedestal = pedestal; }
  const uint16_t getPedestal() { return _noisePedestal; }
  void setAmplitude(uint16_t amplitude) { _noiseAmplitude = amplitude; }
  const uint16_t getAmplitude() { return _noiseAmplitude; }

  // Main generator function: builds frames and calls the fill function.
  void generate(const unsigned long Nframes = 1, char opt = 'b');
  void generate(const std::string& newPrefix, const unsigned long Nframes = 1,
                char opt = 'b');

  // Generator function to create a certain number of frames and place them in
  // the same file.
  void generateSingleFile(const unsigned long Nframes = 1, char opt = 'b');
  void generateSingleFile(const std::string& newPrefix,
                          const unsigned long Nframes = 1, char opt = 'b');

  // Function that attempts to open a file by its name, trying variations with
  // class parameters (_extension, _path, etc.).
  const bool openFile(std::ifstream& strm, const std::string& filename);

  // Overloaded and ranged check functions that absolutely require the FrameGen
  // filename parameters.
  const bool check() {
    return framegen::check(_path + _prefix + "0" + _suffix + _extension);
  }
  const bool check(const unsigned int begin, const unsigned int end);
  const bool check(const unsigned int end) { return check(0, end); }
  const bool checkSingleFile() {
    return framegen::checkSingleFile(getFileName());
  }

  // Overloaded compression/decompression functions.
  const bool compressFile() { return framegen::compressFile(getFileName()); }
  const bool decompressFile() {
    return framegen::decompressFile(getFileName());
  }

  // Overloaded frame print functions.
  bool print(char opt = 'b') {
    return framegen::print(_frame, getFileName(), opt);
  }
  bool print(std::string filename, char opt = 'b') {
    return framegen::print(_frame, filename, opt);
  }
  bool print(std::ofstream& strm, char opt = 'b') {
    return framegen::print(_frame, strm, opt);
  }
};

}  // namespace framegen

#endif /* FRAMEGEN_HPP_ */
