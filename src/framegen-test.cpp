// This is an example program that showcases the various uses of FrameGen.

#include <iostream>
#include <vector>
#include "FrameGen.hpp"

int main(int argc, char* argv[]) {
    // Take a command line argument if available and make a frame generator with the entered noise level (0-2^16).
    framegen::FrameGen* F1;
    if(argc>1)
        F1 = new framegen::FrameGen(atoi(argv[1]));
    else
        F1 = new framegen::FrameGen(32);
    F1->setPath("exampleframes/");
    F1->generate("myfirstframe");
    F1->check(); // Take filename parameters from F1.
    framegen::check("exampleframes/myfirstframe0.frame"); // Do the same thing using just the path and filename.

    // Make a separate generator and use it to put a thousand frames in a single file.
    framegen::FrameGen F2;
    F2.setPath("exampleframes/");
    F2.generateSingleFile("thousand",1000);
    F2.checkSingleFile();
    framegen::checkSingleFile("exampleframes/thousand.frame");

    // Make yet another generator and use it to create frames in separate files.
    framegen::FrameGen F3;
    F3.setPath("exampleframes/lotsoffiles/");
    F3.generate("hundred",100);
    F3.check(100); // Range checks need FrameGen parameters.
    
    // Have a frame generator create a ProtoDUNE header file with a number of frames.
    framegen::FrameGen F4;
    F4.setPath("exampleframes/");
    F4.setExtension(".h");
    F4.generateSingleFile("protodune", 100, 'f');

    // Compress a file and then decompress it again.
    // Since the old files are removed immediately, these two lines effectively do nothing. Comment out the decompression to view a compressed file.
    std::ofstream ofile("exampleframes/test.txt");
    ofile << "This is a test to see whether compression really works.";
    ofile.close();
    framegen::compressFile("exampleframes/test.txt");
    framegen::decompressFile("exampleframes/test.txt.comp");

    // Create a frame, fill it with another file, edit the contents and then print it to another file.
    framegen::Frame Fr;
    Fr.load("exampleframes/thousand.frame", 20);
    for(int i=0; i<256; i++) {
        Fr.set_channel(i/64, (i%64)/8, (i%64)%8, i);
    }
    Fr.resetChecksums();
    Fr.print("exampleframes/printed.frame", 'h'); // The 'h' option prints the frame in hexadecimal notation. (No automatic check on this yet.)
    
    // Test to generate and print a matrix of frames.
    framegen::Frame frame;
    std::vector<framegen::Frame> frameV;
    for(int i=0; i<100; i++)
        frameV.push_back(frame);
    std::vector<std::vector<framegen::Frame>> frameM;
    for(int i=0; i<10; i++)
        frameM.push_back(frameV);
    
    int framenum = 0;
    for(int i=0; i<1000*256; i++) {
        frameM[framenum/100][framenum%100].set_channel((i/64)%4, (i/8)%8, i%8, i);
        
        if((i+1)%256==0) {
            frameM[framenum/100][framenum%100].set_sof(0);
            frameM[framenum/100][framenum%100].set_version(2);
            frameM[framenum/100][framenum%100].set_fiber_no(i%8);
            frameM[framenum/100][framenum%100].set_crate_no(i%(512*5));
            frameM[framenum/100][framenum%100].set_slot_no(i/512);
            frameM[framenum/100][framenum%100].set_z(0);
            frameM[framenum/100][framenum%100].set_timestamp(i*500);
            frameM[framenum/100][framenum%100].set_wib_counter(i/512);
            
            frameM[framenum/100][framenum%100].resetChecksums();
            std::string filename = "exampleframes/range/test" + std::to_string(i/256) + ".frame";
            frameM[framenum/100][framenum%100].print(filename,'b');
            
            framenum++;
        }
    }

    // Test to check whether COLDATA writing and reading correspond.
    framegen::Frame colFrame;
    for(unsigned i=0; i<256; i++)
        colFrame.set_channel(i, 255-i);

    for(unsigned i=0; i<256; i++)
        std::cout << colFrame.channel(i) << std::endl;

    colFrame.print("testframe");
    
    return 0;
}
