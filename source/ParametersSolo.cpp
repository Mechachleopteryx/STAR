#include "ParametersSolo.h"
#include "Parameters.h"
#include "ErrorWarning.h"
#include "streamFuns.h"
#include "SequenceFuns.h"
#include "serviceFuns.cpp"

#include <stdlib.h>

const vector<string> ParametersSolo::featureNames={"Gene","SJ","GeneFull","Transcript3p"};

void ParametersSolo::initialize(Parameters *pPin)
{
    pP=pPin;

    if (typeStr=="None") {
        type=0;
        //solo SAM attributes not allowed
        if (pP->outSAMattrPresent.CR || pP->outSAMattrPresent.CY || pP->outSAMattrPresent.UR || pP->outSAMattrPresent.UY  || pP->outSAMattrPresent.CB  || pP->outSAMattrPresent.UB) {
            ostringstream errOut;
            errOut <<"EXITING because of FATAL INPUT ERROR: --outSAMattributes contains CR/CY/UR/UY tags, but --soloType is not set\n";
            errOut <<"SOLUTION: re-run STAR without these attribures, or with --soloType set\n";
            exitWithError(errOut.str(), std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
        };        
        return;
    } else if (typeStr=="CB_UMI_Simple") {
        type=1;        
        if (umiL > 16) {
            ostringstream errOut;
            errOut << "EXITING because of fatal PARAMETERS error: UMI length is too long: --soloUMIlen="<<umiL<<"\n";
            errOut << "SOLUTION: UMI length cannot be longer than 16";
            exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
        };
        if (cbL > 31) {
            ostringstream errOut;
            errOut << "EXITING because of fatal PARAMETERS error: CB length is too long: --soloCBlen="<<cbL<<"\n";
            errOut << "SOLUTION: CB length cannot be longer than 31";
            exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
        };
        if (bL==1)
            bL=cbL+umiL;
        pP->readNmates=1; //output mates TODO: check that readNmatesIn==2       
    } else if (typeStr=="CB_UMI_Complex") {
        type=2;
    } else  {
        ostringstream errOut;
        errOut << "EXITING because of fatal PARAMETERS error: unrecognized option in --soloType="<<typeStr<<"\n";
        errOut << "SOLUTION: use allowed option: None OR CB_UMI_Simple OR CB_UMI_Complex\n";
        errOut << "Obsolete option Droplet should be replaced with CB_UMI_Simple";
        exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
    };

    if (strandStr=="Unstranded") {
        strand=-1;
    } else if (strandStr=="Forward") {
        strand=0;
    } else if (strandStr=="Reverse") {
        strand=1;
    } else  {
        ostringstream errOut;
        errOut << "EXITING because of fatal PARAMETERS error: unrecognized option in --soloStrand="<<strandStr<<"\n";
        errOut << "SOLUTION: use allowed option: Unstranded OR Forward OR Reverse";
        exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
    };

    featureYes=new bool [featureNames.size()];
    featureInd.resize(featureNames.size(),-1);
    for (auto &fin : featureIn) {
        bool finGood=false;
        for (uint32 ii=0; ii<featureNames.size(); ii++) {
            if (fin==featureNames[ii]) {
                finGood=true;
                featureYes[ii]=true;
                features.push_back(ii);
                featureInd[ii]=features.size()-1;
                break;
            };
        };
        if (!finGood) {
            ostringstream errOut;
            errOut << "EXITING because of fatal PARAMETERS error: unrecognized option in --soloFeatures="<<fin<<"\n";
            errOut << "SOLUTION: use allowed option: ";
            errOut <<featureNames[0]<< "   OR   ";
            for (auto &fname : featureNames)
                errOut << fname <<" ";
            exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
        };
    };

    nFeatures=features.size();

    umiDedupYes.resize(3,false);
    umiDedupColumns.resize(umiDedup.size());
    for (uint32 ii=0; ii<umiDedup.size(); ii++) {
        if (umiDedup[ii]=="1MM_NotCollapsed") {
            umiDedupYes[0]=true;
            umiDedupColumns[ii]=0;
        } else if (umiDedup[ii]=="1MM_All") {
            umiDedupYes[1]=true;
            umiDedupColumns[ii]=1;
        } else if (umiDedup[ii]=="1MM_Directional") {
            umiDedupYes[2]=true;
            umiDedupColumns[ii]=2;
        } else {
            ostringstream errOut;
            errOut << "EXITING because of fatal PARAMETERS error: unrecognized option in --soloUMIdedup="<<umiDedup[ii]<<"\n";
            exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
        };
    };
    ///////////// finished parameters input

    //make output directory if needed
    if ( outFileNames[0].find_last_of("/") < outFileNames[0].size() ) {//need to create dir
        string dir1=pP->outFileNamePrefix+outFileNames[0].substr(0,outFileNames[0].find_last_of("/"));
        if (mkdir(dir1.c_str(),pP->runDirPerm)!=0 && errno!=EEXIST) {
            ostringstream errOut;
            errOut << "EXITING because of fatal OUTPUT FILE error: could not create Solo output directory"<<dir1<<"\n";
            errOut << "SOLUTION: check the path and permisssions";
            exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
        };
    };

    QSbase=33;//TODO make these user-definable
    QSmax=33;
    cbMinP=0.975;

    umiMaskLow=(uint32) ( (((uint64)1)<<umiL) - 1);
    umiMaskHigh=~umiMaskLow;

    //load the CB whitlist
    if (soloCBwhitelist=="-") {
            ostringstream errOut;
            errOut << "EXITING because of FATAL ERROR in INPUT parameters: --soloCBwhitelist is not defined\n";
            errOut << "SOLUTION: in --soloCBwhitelist specify path to and name of the whitelist file, or None for CB demultiplexing without whitelist \n";
            exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_INPUT_FILES, *pP);
    } else if (soloCBwhitelist=="None") {
        cbWLyes=false;
    } else {
        cbWLyes=true;
        ifstream & cbWlStream = ifstrOpen(soloCBwhitelist, ERROR_OUT, "SOLUTION: check the path and permissions of the CB whitelist file: " + soloCBwhitelist, *pP);
        string seq1;
        while (cbWlStream >> seq1) {
            if (seq1.size() != cbL) {
                ostringstream errOut;
                errOut << "EXITING because of FATAL ERROR in input CB whitelist file: "<< soloCBwhitelist <<" the total length of barcode sequence is "  << seq1.size() << " not equal to expected " <<bL <<"\n"  ;
                errOut << "SOLUTION: make sure that the barcode read is the second in --readFilesIn and check that is has the correct formatting\n";
                exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_INPUT_FILES, *pP);
            };
            uint64 cb1;
            //convert to 2-bit format
            if (convertNuclStrToInt64(seq1,cb1)) {
                //cbWL.insert(cb1);
                cbWL.push_back(cb1);
            } else {
                pP->inOut->logMain << "WARNING: CB whitelist sequence contains non-ACGT and is ignored: " << seq1 <<endl;
            };
        };
    };

    std::sort(cbWL.begin(),cbWL.end());
    auto un1=std::unique(cbWL.begin(),cbWL.end());
    cbWL.resize(std::distance(cbWL.begin(),un1));
    //qsort(cbWL.data(),cbWL.size(),sizeof(uint64),funCompareNumbers<uint64>);
    
    time_t rawTime;
    time(&rawTime);
    pP->inOut->logMain << timeMonthDayTime(rawTime) << "Finished reading, sorting and deduplicating CB whitelist sequences: " << cbWL.size() <<endl;

    
    if (!pP->quant.trSAM.yes) {
        pP->quant.yes = true;
        pP->quant.trSAM.yes = true;
        pP->quant.trSAM.bamYes = false;
        pP->quant.trSAM.bamCompression = -2;
        pP->quant.trSAM.indel = true;
        pP->quant.trSAM.softClip = true;
        pP->inOut->logMain << "Turning on Genomic->Transcriptomic coordinate conversion for STARsolo\n";
    };
    
    if (featureYes[2])
        pP->quant.geneFull.yes=true;

    samAttrYes=false;
    samAttrFeature=0;//hard-coded for now to follow Cell Ranger - error correction only done for feature=Gene
    if (pP->outSAMattrPresent.CB || pP->outSAMattrPresent.UB) {
        samAttrYes=true;
        if (!pP->outBAMcoord) {
            ostringstream errOut;
            errOut << "EXITING because of fatal PARAMETERS error: CB and/or UB attributes in --outSAMattributes can only be output in the sorted BAM file.\n";
            errOut << "SOLUTION: re-run STAR with --outSAMtype BAM SortedByCoordinate ...\n";
            exitWithError(errOut.str(),std::cerr, pP->inOut->logMain, EXIT_CODE_PARAMETER, *pP);
        };
    };  
};

void ParametersSolo::umiSwapHalves(uint32 &umi) {
    uint32 high=umi>>(umiL);
    umi &= umiMaskLow; //remove high
    umi <<= (umiL); //move low to high
    umi |= high; //add high
};