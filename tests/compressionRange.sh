for POWER in {0..16}
do
   g++ -std=c++11 -o FrameGen *.cpp
   ./FrameGen $((2**$POWER))

   INITSIZE=$(stat -c%s frames/test.frame)
   gzip frames/test.frame
   FINALSIZE=$(stat -c%s frames/test.frame.gz)

   echo $POWER
   bc <<< "scale=5;$INITSIZE/$FINALSIZE"

   rm frames/test.frame.gz
done
