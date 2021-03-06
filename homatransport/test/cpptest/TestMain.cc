#include <fstream>
#include "application/MsgSizeDistributions.h"

int main() {
    MsgSizeDistributions dctcpDist(
            "../../sizeDistributions/DCTCP_MsgSizeDist.txt",
            1500, MsgSizeDistributions::InterArrivalDist::EXPONENTIAL,
            MsgSizeDistributions::DistributionChoice::DCTCP);
    MsgSizeDistributions facebookDist(
            "../../sizeDistributions/FacebookKeyValueMsgSizeDist.txt",
            1500, MsgSizeDistributions::InterArrivalDist::EXPONENTIAL,
            MsgSizeDistributions::DistributionChoice::FACEBOOK_KEY_VALUE);
    std::ofstream outDctcpStream;
    const char* dctcpFileName = "/tmp/dctcpSampleSizes.txt";
    outDctcpStream.open(dctcpFileName);
    std::ofstream outFacebookStream;
    const char* keyValueFileName = "/tmp/facebookSampleSizes.txt";
    outFacebookStream.open(keyValueFileName);
    for (int i = 0; i < 100000; i++) {
        double nextInterarrivalTime;
        int size;
        dctcpDist.getSizeAndInterarrival(size, nextInterarrivalTime);
        outDctcpStream << size << "\t" << nextInterarrivalTime << "\n"; 
        facebookDist.getSizeAndInterarrival(size, nextInterarrivalTime);
        outFacebookStream << size << "\t" << nextInterarrivalTime <<"\n";
    }
    outDctcpStream.close();
    outFacebookStream.close();
    return 0;
}
