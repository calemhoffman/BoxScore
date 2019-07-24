#ifndef DIGITIZER
#define DIGITIZER

#include <TQObject.h>
#include <RQ_OBJECT.h>
#include "CAENDigitizer.h"
#include "CAENDigitizerType.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <cstring>  //memset
#include <iostream> //cout
#include <fstream>
#include <cmath>
#include <vector>
#include <bitset>
#include <unistd.h>
#include <limits.h>
#include <ctime>

#define MaxNChannels 8

using namespace std;


class Digitizer {
  RQ_OBJECT("Digitizer")  //try to make this class as QObject, but somehow fail
private:

  bool isConnected;
  bool isGood; 
  
  bool AcqRun;

  int boardID; // board identity
  //CAEN_DGTZ_ErrorCode ret1; // return value
  int ret; //return value
  int NChannel; // number of channel

  int Nb; // number of byte
  uint32_t NumEvents[MaxNChannels];
  char *buffer = NULL; // readout buffer
  uint32_t AllocatedSize, BufferSize; 
  CAEN_DGTZ_DPP_PHA_Event_t  *Events[MaxNChannels];  // events buffer

  CAEN_DGTZ_DPP_PHA_Params_t DPPParams;
  int inputDynamicRange[MaxNChannels];
  int energyFineGain[MaxNChannels];
  float chGain[MaxNChannels];
  
  //====================== General Setting
  unsigned long long int ch2ns;
  float DCOffset;
    
  CAEN_DGTZ_ConnectionType LinkType;
  uint32_t VMEBaseAddress;
  CAEN_DGTZ_IOLevel_t IOlev;
  
  CAEN_DGTZ_PulsePolarity_t PulsePolarity;   // Pulse Polarity (this parameter can be individual)
  CAEN_DGTZ_DPP_AcqMode_t AcqMode;
  uint32_t RecordLength;                     // Num of samples of the waveforms (only for waveform mode)
  int EventAggr;                             // number of events in one aggregate (0=automatic), number of event acculated for read-off
  uint PreTriggerSize;     
  uint32_t ChannelMask;                      // Channel enable mask, 0x01, only frist channel, 0xff, all channel  

  //==================== retreved data
  int ECnt[MaxNChannels];
  int TrgCnt[MaxNChannels];
  int PurCnt[MaxNChannels];
  int rawEvCount = 0;
  
  // ===== unsorted data
  vector<ULong64_t> rawTimeStamp;   
  vector<UInt_t> rawEnergy;
  vector<int> rawChannel;
  

public:
  Digitizer(int ID);
  ~Digitizer();
  
  void SetChannelMask(uint32_t mask);
  void SetDCOffset(int ch , float offset);
  
  int GetNumRawEvent() {return rawEvCount;}
  vector<ULong64_t> GetRawTimeStamp() {return rawTimeStamp;}
  vector<UInt_t> GetRawEnergy() {return rawEnergy;}
  vector<int> GetRawChannel() {return rawChannel;}
  
  void GetChannelSetting(int ch);
  uint32_t GetChannelMask() const {return ChannelMask;}
  int GetNChannel(){ return NChannel;}
  
  bool IsGood() {return isGood;}
  void GetBoardConfiguration();
  
  int ProgramDigitizer();
  void ReadChannelSetting (const int ch, string fileName);
  void ReadGeneralSetting(string fileName);
  
  void StopACQ();
  void StartACQ();
  
  void ReadData();
  
};

Digitizer::Digitizer(int ID){
  
  //================== initialization
  boardID = ID;
  NChannel = 0;
  
  AcqRun = false;
  
  ch2ns = 2; // 1 channel = 2 ns
  DCOffset = 0.2;
  
  Nb = 0;
  

  for ( int i = 0; i < MaxNChannels ; i++ ) {
    inputDynamicRange[i] = 0;
    chGain[i] = 1.0;
    energyFineGain[i] = 100;
    NumEvents[i] = 0; 
  }
  
  memset(&DPPParams, 0, sizeof(CAEN_DGTZ_DPP_PHA_Params_t));
  
  //----------------- Communication Parameters 
  LinkType = CAEN_DGTZ_USB;     // Link Type
  VMEBaseAddress = 0;           // For direct USB connection, VMEBaseAddress must be 0
  IOlev = CAEN_DGTZ_IOLevel_NIM;
  
  //--------------Acquisition parameters 
  //AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_Mixed;          // CAEN_DGTZ_DPP_ACQ_MODE_List or CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope
  AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_List;             // CAEN_DGTZ_DPP_ACQ_MODE_List or CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope
  RecordLength = 20000;
  PreTriggerSize = 2000;
  ChannelMask = 0xff;
  EventAggr = 1;
  PulsePolarity = CAEN_DGTZ_PulsePolarityPositive; 
  //PulsePolarity = CAEN_DGTZ_PulsePolarityNegative; 
  
  //===================== end of initization

  /* *********************************************** */
  /* Open the digitizer and read board information   */
  /* *********************************************** */
  
  printf("============= Opening Digitizer at Board %d \n", boardID);
  
  isConnected = false;
  isGood = true;
  
  ret = (int) CAEN_DGTZ_OpenDigitizer(LinkType, boardID, 0, VMEBaseAddress, &boardID);
  if (ret != 0) {
    printf("Can't open digitizer\n");
    isGood = false;
  }else{
    //----- Getting Board Info
    CAEN_DGTZ_BoardInfo_t BoardInfo;
    ret = (int) CAEN_DGTZ_GetInfo(boardID, &BoardInfo);
    if (ret != 0) {
      printf("Can't read board info\n");
      isGood = false;
    }else{
      isConnected = true;
      printf("Connected to CAEN Digitizer Model %s, recognized as board %d\n", BoardInfo.ModelName, boardID);
      NChannel = BoardInfo.Channels;
      printf("Number of Channels : %d\n", NChannel);
      printf("SerialNumber : %d\n", BoardInfo.SerialNumber);
      printf("ROC FPGA Release is %s\n", BoardInfo.ROC_FirmwareRel);
      printf("AMC FPGA Release is %s\n", BoardInfo.AMC_FirmwareRel);
      
      int MajorNumber;
      sscanf(BoardInfo.AMC_FirmwareRel, "%d", &MajorNumber);
      if (MajorNumber != V1730_DPP_PHA_CODE) {
        printf("This digitizer does not have DPP-PHA firmware\n");
      }
    }
  }
  
  /* *********************************************** */
  /* Get Channel Setting and Set Digitizer           */
  /* *********************************************** */
  if( isGood ){
    printf("=================== reading Channel setting \n");
    for(int ch = 0; ch < NChannel; ch ++ ) {
      if ( ChannelMask & ( 1 << ch) ) {
        ReadChannelSetting(ch, "setting_" + to_string(ch) + ".txt");
      }
    }
    printf("====================================== \n");
  
    //============= Program the digitizer (see function ProgramDigitizer) 
    ret = (CAEN_DGTZ_ErrorCode)ProgramDigitizer();
    if (ret != 0) {
      printf("Failed to program the digitizer\n");
      isGood = false;
    }
  }
  
  if( isGood ) {
    /* WARNING: The mallocs MUST be done after the digitizer programming,
    because the following functions needs to know the digitizer configuration
    to allocate the right memory amount */
    // Allocate memory for the readout buffer 
    ret = CAEN_DGTZ_MallocReadoutBuffer(boardID, &buffer, &AllocatedSize);
    // Allocate memory for the events
    ret |= CAEN_DGTZ_MallocDPPEvents(boardID, reinterpret_cast<void**>(&Events), &AllocatedSize) ;     
    
    if (ret != 0) {
      printf("Can't allocate memory buffers\n");
      CAEN_DGTZ_SWStopAcquisition(boardID);
      CAEN_DGTZ_CloseDigitizer(boardID);
      CAEN_DGTZ_FreeReadoutBuffer(&buffer);
      CAEN_DGTZ_FreeDPPEvents(boardID, reinterpret_cast<void**>(&Events));
    }else{
      printf("====== Allocated memory for communication.\n");
    }
    
  }
  
  printf("################### Digitizer is ready \n");
  
}


Digitizer::~Digitizer(){
  
  
  /* stop the acquisition, close the device and free the buffers */
  CAEN_DGTZ_SWStopAcquisition(boardID);
  CAEN_DGTZ_CloseDigitizer(boardID);
  CAEN_DGTZ_FreeReadoutBuffer(&buffer);
  CAEN_DGTZ_FreeDPPEvents(boardID, reinterpret_cast<void**>(&Events));
  
  printf("Close digitizer\n");
  
  for(int ch = 0; ch < MaxNChannels; ch++){
    delete Events[ch];
  }
  
  delete buffer;
}

void Digitizer::SetChannelMask(uint32_t mask){ 
  ChannelMask = mask; 
  if( isConnected ){
    ret = CAEN_DGTZ_SetChannelEnableMask(boardID, ChannelMask);
    if( ret == 0 ){
      printf("---- ChannelMask changed to %d \n", ChannelMask);
    }else{
      printf("---- Fail to change ChannelMask \n");
    }
  }
}


void Digitizer::SetDCOffset(int ch, float offset){
  DCOffset = offset;
  
  ret = CAEN_DGTZ_SetChannelDCOffset(boardID, ch, uint( 0xffff * DCOffset ));
  
  if( ret == 0 ){
    printf("---- DC Offset of CH : %d is set to %f \n", ch, DCOffset);
  }else{
    printf("---- Fail to Set DC Offset of CH : %d \n", ch);
  }
  
}



void Digitizer::GetBoardConfiguration(){
  uint32_t * value = new uint32_t[1];
  CAEN_DGTZ_ReadRegister(boardID, 0x8000 , value);
  printf("                        32  28  24  20  16  12   8   4   0\n");
  printf("                         |   |   |   |   |   |   |   |   |\n");
  cout <<" Board Configuration  : 0b" << bitset<32>(value[0]) << endl;
  printf("                Bit[ 0] = Auto Data Flush   \n");
  printf("                Bit[16] = WaveForm Recording   \n");
  printf("                Bit[17] = Extended Time Tag   \n");
  printf("                Bit[18] = Record Time Stamp   \n");
  printf("                Bit[19] = Record Energy   \n");
  printf("====================================== \n");
}

void Digitizer::GetChannelSetting(int ch){
  
  uint32_t * value = new uint32_t[MaxNChannels];
  printf("================================================\n");
  printf("================ Getting setting for channel %d \n", ch);
  printf("================================================\n");
  //DPP algorithm Control
  CAEN_DGTZ_ReadRegister(boardID, 0x1080 + (ch << 8), value);
  printf("                          32  28  24  20  16  12   8   4   0\n");
  printf("                           |   |   |   |   |   |   |   |   |\n");
  cout <<" DPP algorithm Control  : 0b" << bitset<32>(value[0]) << endl;
  
  int trapRescaling = int(value[0]) & 31 ;
  int polarity = int(value[0] >> 16); //in bit[16]
  int baseline = int(value[0] >> 20) ; // in bit[22:20]
  int NsPeak = int(value[0] >> 12); // in bit[13:12]
  //DPP algorithm Control 2
  CAEN_DGTZ_ReadRegister(boardID, 0x10A0 + (ch << 8), value);
  cout <<" DPP algorithm Control 2: 0b" << bitset<32>(value[0]) << endl;
  
  printf("* = multiple of 8 \n");
  
  printf("==========----- input \n");
  CAEN_DGTZ_ReadRegister(boardID, 0x1020 + (ch << 8), value); printf("%20s  %d \n", "Record Length",  value[0] * 8); //Record length
  CAEN_DGTZ_ReadRegister(boardID, 0x1038 + (ch << 8), value); printf("%20s  %d \n", "Pre-tigger",  value[0] * 4); //Pre-trigger
  printf("%20s  %s \n", "polarity",  (polarity & 1) ==  0 ? "Positive" : "negative"); //Polarity
  printf("%20s  %.0f sample \n", "Ns baseline",  pow(4, 1 + baseline & 7)); //Ns baseline
  CAEN_DGTZ_ReadRegister(boardID, 0x1098 + (ch << 8), value); printf("%20s  %.2f %% \n", "DC offset",  value[0] * 100./ int(0xffff) ); //DC offset
  CAEN_DGTZ_ReadRegister(boardID, 0x1028 + (ch << 8), value); printf("%20s  %.1f Vpp \n", "input Dynamic",  value[0] == 0 ? 2 : 0.5); //InputDynamic
  
  printf("==========----- discriminator \n");
  CAEN_DGTZ_ReadRegister(boardID, 0x106C + (ch << 8), value); printf("%20s  %d LSB\n", "Threshold",  value[0]); //Threshold
  CAEN_DGTZ_ReadRegister(boardID, 0x1074 + (ch << 8), value); printf("%20s  %d ns \n", "trigger hold off *",  value[0] * 8); //Trigger Hold off
  CAEN_DGTZ_ReadRegister(boardID, 0x1054 + (ch << 8), value); printf("%20s  %d sample \n", "Fast Dis. smoothing",  value[0] *2 ); //Fast Discriminator smoothing
  CAEN_DGTZ_ReadRegister(boardID, 0x1058 + (ch << 8), value); printf("%20s  %d ch \n", "Input rise time *",  value[0] * 8); //Input rise time
  
  printf("==========----- Trapezoid \n");
  CAEN_DGTZ_ReadRegister(boardID, 0x1080 + (ch << 8), value); printf("%20s  %d bit = Floor( rise * decay / 64 )\n", "Trap. Rescaling",  trapRescaling ); //Trap. Rescaling Factor
  CAEN_DGTZ_ReadRegister(boardID, 0x105C + (ch << 8), value); printf("%20s  %d ns \n", "Trap. rise time *",  value[0] * 8 ); //Trap. rise time
  CAEN_DGTZ_ReadRegister(boardID, 0x1060 + (ch << 8), value); 
  int flatTopTime = value[0] * 8;
  printf("%20s  %d ns \n", "Trap. flat time *",  flatTopTime); //Trap. flat time
  CAEN_DGTZ_ReadRegister(boardID, 0x1020 + (ch << 8), value); printf("%20s  %d ns \n", "Trap. pole zero *",  value[0] * 8); //Trap. pole zero
  CAEN_DGTZ_ReadRegister(boardID, 0x1068 + (ch << 8), value); printf("%20s  %d ns \n", "Decay time *",  value[0] * 8); //Trap. pole zero
  CAEN_DGTZ_ReadRegister(boardID, 0x1064 + (ch << 8), value); printf("%20s  %d ns = %.2f %% \n", "peaking time *",  value[0] * 8, value[0] * 800. / flatTopTime ); //Peaking time
  printf("%20s  %.0f sample\n", "Ns peak",  pow(4, NsPeak & 3)); //Ns peak
  CAEN_DGTZ_ReadRegister(boardID, 0x1078 + (ch << 8), value); printf("%20s  %d ns \n", "Peak hole off*",  value[0] * 8 ); //Peak hold off
  
  printf("==========----- Other \n");
  CAEN_DGTZ_ReadRegister(boardID, 0x104C + (ch << 8), value); printf("%20s  %d \n", "Energy fine gain",  value[0]); //Energy fine gain
    
  
}

int Digitizer::ProgramDigitizer(){
  /* This function uses the CAENDigitizer API functions to perform the digitizer's initial configuration */
    int ret = 0;

    /* Reset the digitizer */
    ret |= CAEN_DGTZ_Reset(boardID);

    if (ret) {
        printf("ERROR: can't reset the digitizer.\n");
        return -1;
    }
    
    ret |= CAEN_DGTZ_WriteRegister(boardID, 0x8000, 0x01000114);  // Channel Control Reg (indiv trg, seq readout) ??

    /* Set the DPP acquisition mode
    This setting affects the modes Mixed and List (see CAEN_DGTZ_DPP_AcqMode_t definition for details)
    CAEN_DGTZ_DPP_SAVE_PARAM_EnergyOnly        Only energy (DPP-PHA) or charge (DPP-PSD/DPP-CI v2) is returned
    CAEN_DGTZ_DPP_SAVE_PARAM_TimeOnly        Only time is returned
    CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime    Both energy/charge and time are returned
    CAEN_DGTZ_DPP_SAVE_PARAM_None            No histogram data is returned */
    ret |= CAEN_DGTZ_SetDPPAcquisitionMode(boardID, AcqMode, CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime);
    
    // Set the digitizer acquisition mode (CAEN_DGTZ_SW_CONTROLLED or CAEN_DGTZ_S_IN_CONTROLLED)
    ret |= CAEN_DGTZ_SetAcquisitionMode(boardID, CAEN_DGTZ_SW_CONTROLLED);
    
    // Set the number of samples for each waveform
    ret |= CAEN_DGTZ_SetRecordLength(boardID, RecordLength);

    // Set the I/O level (CAEN_DGTZ_IOLevel_NIM or CAEN_DGTZ_IOLevel_TTL)
    ret |= CAEN_DGTZ_SetIOLevel(boardID, IOlev);

    /* Set the digitizer's behaviour when an external trigger arrives:

    CAEN_DGTZ_TRGMODE_DISABLED: do nothing
    CAEN_DGTZ_TRGMODE_EXTOUT_ONLY: generate the Trigger Output signal
    CAEN_DGTZ_TRGMODE_ACQ_ONLY = generate acquisition trigger
    CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT = generate both Trigger Output and acquisition trigger

    see CAENDigitizer user manual, chapter "Trigger configuration" for details */
    ret |= CAEN_DGTZ_SetExtTriggerInputMode(boardID, CAEN_DGTZ_TRGMODE_ACQ_ONLY);

    // Set the enabled channels
    ret |= CAEN_DGTZ_SetChannelEnableMask(boardID, ChannelMask);

    // Set how many events to accumulate in the board memory before being available for readout
    ret |= CAEN_DGTZ_SetDPPEventAggregation(boardID, EventAggr, 0);
    
    /* Set the mode used to syncronize the acquisition between different boards.
    In this example the sync is disabled */
    ret |= CAEN_DGTZ_SetRunSynchronizationMode(boardID, CAEN_DGTZ_RUN_SYNC_Disabled);
    
    // Set the DPP specific parameters for the channels in the given channelMask
    ret |= CAEN_DGTZ_SetDPPParameters(boardID, ChannelMask, &DPPParams);
    
    // Set Extras 2 to enable, this override Accusition mode, focring list mode
    uint32_t value = 0x10E0114;
    ret |= CAEN_DGTZ_WriteRegister(boardID, 0x8000 , value );
    
    for(int i=0; i<MaxNChannels; i++) {
        if (ChannelMask & (1<<i)) {
            // Set a DC offset to the input signal to adapt it to digitizer's dynamic range
            //ret |= CAEN_DGTZ_SetChannelDCOffset(boardID, i, 0x3333); // 20%
            ret |= CAEN_DGTZ_SetChannelDCOffset(boardID, i, uint( 0xffff * DCOffset ));
            
            // Set the Pre-Trigger size (in samples)
            ret |= CAEN_DGTZ_SetDPPPreTriggerSize(boardID, i, PreTriggerSize);
            
            // Set the polarity for the given channel (CAEN_DGTZ_PulsePolarityPositive or CAEN_DGTZ_PulsePolarityNegative)
            ret |= CAEN_DGTZ_SetChannelPulsePolarity(boardID, i, PulsePolarity);
            
            // Set InputDynamic Range
            ret |= CAEN_DGTZ_WriteRegister(boardID, 0x1028 +  (i<<8), inputDynamicRange[i]);
            
            // Set Energy Fine gain
            ret |= CAEN_DGTZ_WriteRegister(boardID, 0x104C +  (i<<8), energyFineGain[i]);
            
            // read the register to check the input is correct
            //uint32_t * value = new uint32_t[8];
            //ret = CAEN_DGTZ_ReadRegister(boardID, 0x1028 + (i << 8), value);
            //printf(" InputDynamic Range (ch:%d): %d \n", i, value[0]);
        }
    }

    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(boardID, ANALOG_TRACE_1, CAEN_DGTZ_DPP_VIRTUALPROBE_Delta2);
    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(boardID, ANALOG_TRACE_2, CAEN_DGTZ_DPP_VIRTUALPROBE_Input);
    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(boardID, DIGITAL_TRACE_1, CAEN_DGTZ_DPP_DIGITALPROBE_Peaking);

    if (ret) {
        printf("Warning: errors found during the programming of the digitizer.\nSome settings may not be executed\n");
        return ret;
    } else {
        return 0;
    }
  
}

void Digitizer::ReadChannelSetting (const int ch, string fileName) {
  
  ifstream file_in;
  file_in.open(fileName.c_str(), ios::in);

  if( !file_in){
    printf("channel: %d | default.\n", ch);
    DPPParams.thr[ch]   = 100;      // Trigger Threshold (in LSB)
    DPPParams.trgho[ch] = 1200;     // Trigger Hold Off
    DPPParams.a[ch]     = 4;        // Trigger Filter smoothing factor (number of samples to average for RC-CR2 filter) Options: 1; 2; 4; 8; 16; 32
    DPPParams.b[ch]     = 200;      // Input Signal Rise time (ns) 
    
    DPPParams.k[ch]    = 3000;     // Trapezoid Rise Time (ns) 
    DPPParams.m[ch]    = 900;      // Trapezoid Flat Top  (ns) 
    DPPParams.M[ch]    = 50000;    // Decay Time Constant (ns) 
    DPPParams.ftd[ch]  = 500;      // Flat top delay (peaking time) (ns) 
    DPPParams.nspk[ch] = 0;        // Peak mean (number of samples to average for trapezoid height calculation). Options: 0-> 1 sample; 1->4 samples; 2->16 samples; 3->64 samples
    DPPParams.pkho[ch] = 2000;    // peak holdoff (ns)
    
    DPPParams.nsbl[ch]    = 4;        // number of samples for baseline average calculation. Options: 1->16 samples; 2->64 samples; 3->256 samples; 4->1024 samples; 5->4096 samples; 6->16384 samples
    inputDynamicRange[ch] = 0;       // input dynamic range, 0 = 2 Vpp, 1 = 0.5 Vpp

    energyFineGain[ch]       = 10;      // Energy Fine gain
    DPPParams.blho[ch]       = 500;     // Baseline holdoff (ns)        
    DPPParams.enf[ch]        = 1.0;     // Energy Normalization Factor
    DPPParams.decimation[ch] = 0;       // decimation (the input signal samples are averaged within this number of samples): 0 ->disabled; 1->2 samples; 2->4 samples; 3->8 samples
    DPPParams.dgain[ch]      = 0;       // decimation gain. Options: 0->DigitalGain=1; 1->DigitalGain=2 (only with decimation >= 2samples); 2->DigitalGain=4 (only with decimation >= 4samples); 3->DigitalGain=8( only with decimation = 8samples).
    DPPParams.trgwin[ch]     = 0;       // Enable Rise time Discrimination. Options: 0->disabled; 1->enabled
    DPPParams.twwdt[ch]      = 100;     // Rise Time Validation Window (ns)
    
    chGain[ch] = -1.0;      // gain of the channel; if -1, default based on input-dynamic range;
    
  }else{
    printf("channel: %d | %s.\n", ch, fileName.c_str());
    string line;
    int count = 0;
    while( file_in.good()){
      getline(file_in, line);
      size_t pos = line.find("//");
      if( pos > 1 ){
        if( count ==  0 ) DPPParams.thr[ch]        = atoi(line.substr(0, pos).c_str());
        if( count ==  1 ) DPPParams.trgho[ch]      = atoi(line.substr(0, pos).c_str());
        if( count ==  2 ) DPPParams.a[ch]          = atoi(line.substr(0, pos).c_str());
        if( count ==  3 ) DPPParams.b[ch]          = atoi(line.substr(0, pos).c_str());
        if( count ==  4 ) DPPParams.k[ch]          = atoi(line.substr(0, pos).c_str());
        if( count ==  5 ) DPPParams.m[ch]          = atoi(line.substr(0, pos).c_str());
        if( count ==  6 ) DPPParams.M[ch]          = atoi(line.substr(0, pos).c_str());
        if( count ==  7 ) DPPParams.ftd[ch]        = atoi(line.substr(0, pos).c_str());
        if( count ==  8 ) DPPParams.nspk[ch]       = atoi(line.substr(0, pos).c_str());
        if( count ==  9 ) DPPParams.pkho[ch]       = atoi(line.substr(0, pos).c_str());
        if( count == 10 ) DPPParams.nsbl[ch]       = atoi(line.substr(0, pos).c_str());
        if( count == 11 ) inputDynamicRange[ch]    = atoi(line.substr(0, pos).c_str());
        if( count == 12 ) energyFineGain[ch]       = atoi(line.substr(0, pos).c_str());
        if( count == 13 ) DPPParams.blho[ch]       = atoi(line.substr(0, pos).c_str());
        if( count == 14 ) DPPParams.enf[ch]        = atof(line.substr(0, pos).c_str());
        if( count == 15 ) DPPParams.decimation[ch] = atoi(line.substr(0, pos).c_str());
        if( count == 16 ) DPPParams.dgain[ch]      = atoi(line.substr(0, pos).c_str());
        if( count == 17 ) DPPParams.trgwin[ch]     = atoi(line.substr(0, pos).c_str());
        if( count == 18 ) DPPParams.twwdt[ch]      = atoi(line.substr(0, pos).c_str());
        count++;
      }
    }
  }
  
};

void Digitizer::ReadGeneralSetting(string fileName){
  const int numPara = 17;
  
  ifstream file_in;
  file_in.open(fileName.c_str(), ios::in);
  
  printf("====================================== \n");

  if( !file_in){
    printf("====== Using Built-in General Setting.\n");
  }else{
    printf("====== Reading General Setting from  %s.\n", fileName.c_str());
    string line;
    int count = 0;
    while( file_in.good()){
      getline(file_in, line);
      size_t pos = line.find("//");
      if( pos > 1 ){
        if( count > numPara - 1) break;
        
        if( count == 0 )  DCOffset = atof(line.substr(0, pos).c_str());
        if( count == 1 )  {
          if( line.substr(0, 4) == "true" ) {
            PulsePolarity = CAEN_DGTZ_PulsePolarityPositive; 
          }else{
            PulsePolarity = CAEN_DGTZ_PulsePolarityNegative; 
          }
        }
        if( count == 2  )   RecordLength = atoi(line.substr(0, pos).c_str());
        if( count == 3  ) PreTriggerSize = atoi(line.substr(0, pos).c_str());
        count ++;
      }
    }
    
    //=============== print setting
    printf(" %-20s  %.3f (0x%04x)\n", "DC offset", DCOffset, uint( 0xffff * DCOffset ));
    printf(" %-20s  %d\n", "Pulse Polarity", PulsePolarity);
    printf(" %-20s  %d ch\n", "Record Lenght", RecordLength);
    printf(" %-20s  %d ch\n", "Pre-Trigger Size", PreTriggerSize);
    printf("====================================== \n");
    
  }

  return;
  
}


void Digitizer::StartACQ(){
  
  CAEN_DGTZ_SWStartAcquisition(boardID);
  printf("Acquisition Started for Board %d\n", boardID);
  AcqRun = true;
  
}

void Digitizer::ReadData(){
  /* Read data from the board */
  ret = CAEN_DGTZ_ReadData(boardID, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
  if (BufferSize == 0) return;
  
  Nb += BufferSize;
  ret |= (CAEN_DGTZ_ErrorCode) CAEN_DGTZ_GetDPPEvents(boardID, buffer, BufferSize, reinterpret_cast<void**>(&Events), NumEvents);
  if (ret) {
    printf("Data Error: %d\n", ret);
    CAEN_DGTZ_SWStopAcquisition(boardID);
    CAEN_DGTZ_CloseDigitizer(boardID);
    CAEN_DGTZ_FreeReadoutBuffer(&buffer);
    CAEN_DGTZ_FreeDPPEvents(boardID, reinterpret_cast<void**>(&Events));
    return;
  }
  
  /* Analyze data */
  for (int ch = 0; ch < MaxNChannels; ch++) {
    if (!(ChannelMask & (1<<ch))) continue;
    //printf("------------------------ %d \n", ch);
    for (int ev = 0; ev < NumEvents[ch]; ev++) {
      TrgCnt[ch]++;
      
      if (Events[ch][ev].Energy > 0) {
        ECnt[ch]++;
          
        ULong64_t timetag = (ULong64_t) Events[ch][ev].TimeTag;
        ULong64_t rollOver = Events[ch][ev].Extras2 >> 16;
        rollOver = rollOver << 31;
        timetag  += rollOver ;
        
        //printf("%llu | %llu | %llu \n", Events[ch][ev].Extras2 , rollOver >> 32, timetag);
                    
        rawEvCount ++;
        
        rawChannel.push_back(ch);
        rawEnergy.push_back(Events[ch][ev].Energy);
        rawTimeStamp.push_back(timetag);
        
        
      } else { /* PileUp */
          PurCnt[ch]++;
      }
        
    } // loop on events
    
  } // loop on channels
  
  
}

void Digitizer::StopACQ(){
  
  CAEN_DGTZ_SWStopAcquisition(boardID); 
  printf("\n====== Acquisition STOPPED for Board %d\n", boardID);
  AcqRun = false;
}


#endif