//  Code to implement a Selective Repeat protocol on L4
//  MBP111.0138.B16
//  System Serial: C02P4SP9G3QH
//  sr.cpp
//  Created by Abhishek Gautam on 3/26/2016
//  agautam2@buffalo.edu
//  University at Buffalo, The State University of New York.
//  Copyright © 2016 Gautam. All rights reserved.

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

#include "simulator.h"
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

using namespace std;

int A_application = 0;
int A_transport = 0;
int B_application = 0;
int B_transport = 0;
float time_local = 0;

/*Define variables specific to the SR protocol below this space*/

/*Basic variables pertaining to SR*/
float timerIncrement;
int windowSize;
int slidingWindow;
int senderBuffer=20000;
int receiverBuffer=20000;
int baseA;
int baseB;
int seekerA;
int seekerB;
bool repeatedPacket;
float timeElapsed;
/*variables for extra stats information*/
int denominator;
int at;
int ackWithInvalidSeqNum;
int corruptPacket;
int corruptAck;
int ackCount;
/*extra struct types used*/
struct msg storeLastMsg;
struct pkt storeLastAck;
struct pkt ackFromB;

/*Define variables specific to the SR protocol above this space*/


/*How to add more fields to existing struct without
 modifying it ? (no alterations allowed to simulator.h and simulator.cpp),
 logic read & understood from -
 https://bytes.com/topic/c/answers/435918-add-new-member-struct
 AND How to use nested structures, read & understood from -
 http://www.c4learn.com/c-programming/c-nested-structure/ */

struct packInfoA
{
    struct pkt packet;
    bool partOfBuffer;
    float sendingTime;
    bool pushedtToL3;
    bool pushedtToL5;
};

struct packInfoB
{
    struct pkt packet;
    bool partOfBuffer;
};

/* struct memory allocation and array handling - http://www.programiz.com/c-programming/c-structures-pointers*/
struct packInfoA *ptrA= (struct packInfoA*)malloc(senderBuffer*sizeof(struct packInfoA));
struct packInfoB *ptrB= (struct packInfoB*)malloc(receiverBuffer*sizeof(struct packInfoB));

int calculatePayloadChecksum(struct pkt packPayload)          //calculates char by char checksum for payload
{
    
    int payloadChecksum = 0;
    int i=0;
    while(i<(sizeof(packPayload.payload)))
    {
        payloadChecksum=payloadChecksum+packPayload.payload[i];
        i++;
    }
    return payloadChecksum;
}

int calculateChecksum(struct pkt pack)                        //adds payload checksum value to ack and seq => consolidates checksum
{
    int initChecksum=0;
    initChecksum = (pack.acknum+pack.seqnum+calculatePayloadChecksum(pack))*2;
    return initChecksum;
}

/*various setters*/
int setToZero(int a)
{
    a=0;
    return a;
}

bool setToFalse(bool a)
{
    a=false;
    return a;
}

bool setToTrue(bool a)
{
    a=true;
    return a;
}
/*various setters*/

int isCorrupt(int a, int b)                                //checks if the packet is corrupt on the basis of two checksum values
{
    if(a==b)
    {
        //printf("@@TEST - GOOD PACKET@@\n");
        return 1;   //packet is not corrupt
    }
    else
    {
        //printf("@@TEST - BAD PACKET@@\n");
        return 0;   //packet is corrupt
    }
}

float performanceCalculator(int a, int b)
{
    return (float) a/b;
}

void adaptiveTimeout()
{
    //printf("ackcount: %d\n",ackCount);
    //printf("A_app count: %d\n",denominator);
    float eff=performanceCalculator(ackCount, denominator);
    //printf("eff: %f\n",eff);
    if(eff>=1.0)
    {
        timerIncrement=12.0;
        printf("selected - 12\n");
        return;
    }
    if(eff>0.6 && eff<1.0)
    {
        timerIncrement=11.0;
        printf("selected - 11\n");
        return;
    }
    if(eff>0.30 && eff<.60)
    {
        timerIncrement=10.0;
        printf("selected - 10\n");
        return;
    }
    if(eff<0.3)
    {
        timerIncrement=9.0;
        printf("selected - 9\n");
        return;
    }
    else
    {
        return;
    }
}

void storeAndPacketize(int seek, struct pkt packet, struct msg message)
{
    /*Why use strncpy instead of strcpy understood from -
     http://stackoverflow.com/questions/1258550/why-should-you-use-strncpy-instead-of-strcpy */
    /*use strncpy. strcpy gives seg fault*/
    
    /*How to add more fields to existing struct without
     modifying it ? (no alterations allowed to simulator.h and simulator.cpp),
     logic read & understood from -
     https://bytes.com/topic/c/answers/435918-add-new-member-struct
     AND How to use nested structures, read & understood from -
     http://www.c4learn.com/c-programming/c-nested-structure/ */
    
    strncpy(packet.payload,message.data,sizeof(packet.payload));
    packet.checksum=calculateChecksum(packet);
    
    /*accessing struct members - http://www.c4learn.com/c-programming/c-accessing-element-in-structure-array/ and C++ the complete reference textbook*/
    
   
    ptrA[seek].pushedtToL3=setToFalse(ptrA[seek].pushedtToL3);
    ptrA[seek].partOfBuffer=setToTrue(ptrA[seek].partOfBuffer);
    ptrA[seek].pushedtToL5=setToFalse(ptrA[seek].pushedtToL5);
    ptrA[seek].packet=packet;
    
}


/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message)
{
    /*what to do when data received from above? - ROSS KUROSE - Page 226*/
    //printf("--------------------Inside A_Output---------------------\n");
    printf("-------------------------------%d------------------------------\n",seekerA);
    denominator++;
    pkt packet;
    setToZero(packet.checksum);
    setToZero(packet.acknum);
    packet.seqnum=seekerA;
    storeAndPacketize(seekerA, packet, message);
    slidingWindow=baseA+windowSize;
    
    if(seekerA>slidingWindow)
    {
        //simply update seeker so that new incoming data is packetized and stored at new seeker poistion
        seekerA++;
        //printf("value of seeker @: %d\n", seekerA);
    }
    else
    {
        int y=baseA;
        while(y<slidingWindow)
        {
           if(ptrA[y].partOfBuffer==true)
            {
                if(ptrA[y].pushedtToL3==false && ptrA[y].pushedtToL5==false)
                {
                        ptrA[y].sendingTime=get_sim_time(); //each packets has its own sent time. This will be used to monitor packet loss.
                        //send packet
                        tolayer3(0, ptrA[y].packet);
                        /*interrupt every unit time to check which packets did not make it*/
                        //printf("packet %d sent with checksum %d \n", ptrA[y].packet.seqnum, ptrA[y].packet.checksum);
                        ptrA[y].pushedtToL3=setToTrue(ptrA[y].pushedtToL3);
                        at++;
                        seekerA++;
                        //printf("value of seeker #: %d\n", seekerA);
                        if(at%5==0)
                        {
                            adaptiveTimeout();
                        }
                        
                    
                }
            }
            y++;//increment while
        }
        
    }//end of else
    //printf("exiting A_output\n");
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
    //printf("--------------------Inside A_Input---------------------\n");
    int payloadChecksum=0;
    int verifyChecksum=0;
    verifyChecksum=packet.acknum+packet.seqnum;
    
    int i=0;
    while(i<(sizeof(packet.payload)))
    {
        payloadChecksum=payloadChecksum+packet.payload[i];
        i++;
    }
    verifyChecksum=(verifyChecksum+payloadChecksum)*2;
    //printf("@@@@@@@@@@@@@@@ Sent from B : %d\n",packet.checksum);
    //printf("@@@@@@@@@@@@@@@ Received at A : %d\n",verifyChecksum);
    
    
    int goodOrBad = isCorrupt(packet.checksum, verifyChecksum);
    slidingWindow=baseA+windowSize;
    
    if(goodOrBad==1)
    {
        //printf("ack is good moving forward\n");
        if(packet.acknum>=baseA)
        {
            if(packet.acknum<slidingWindow)
            {
                //printf("A has received a Valid Ack # %d with checksum %d \n", packet.acknum,packet.checksum);
                //control will reach here if B reveives a valid ack
                ackCount++;
                
                /*
                 Next steps would be:
                 1. Mark packet as delivered.
                 2. Check for other packets. if delivered, move window
                 3. send packets that were stored due to high incoming rate from L5
                 */
                
                //STEP 1
                //setToFalse(ptrA[packet.acknum].pushedtToL5);
                
                //printf("previous #STATUS: %d\n",ptrA[packet.acknum].pushedtToL5);
                ptrA[packet.acknum].pushedtToL5=setToTrue(ptrA[packet.acknum].pushedtToL5);
                //printf("packet marked as delivered, #STATUS %d\n",ptrA[packet.acknum].pushedtToL5);
                
                //STEP 2
                int y = baseA;
                while(y<=slidingWindow)
                {
                    if(ptrA[y].pushedtToL5==false)
                    {
                        //baseA++;
                        break;
                        //printf("window moved\n");
                        //printf("base A is: %d\n",baseA);
                        
                    }
                    else
                    {
                        baseA++;
                        //printf("TEST\n");
                        //break;
                    }
                    y++;//increment while
                }
                
                //STEP 3
                int g=baseA;
                while(g<slidingWindow)
                {
                    if(ptrA[g].partOfBuffer==true)
                    {
                        if(ptrA[g].pushedtToL3==false && ptrA[g].pushedtToL5==false)
                        {
                                ptrA[g].sendingTime=get_sim_time();
                                tolayer3(0, ptrA[g].packet);
                                //printf("buffered packet(s) were stored\n");
                                //setToFalse(ptrA[g].sent);
                                ptrA[g].pushedtToL3=setToTrue(ptrA[g].pushedtToL3);
                            
                                if(at%5==0)
                                {
                                    adaptiveTimeout();
                                }
                            
                        }
                    }
                    g++; //increment while
                }
                
            }
        }
        
    }// if of goodOrBad ends
    else
    {
        if(goodOrBad==0)
        {
            //printf("A has received a INValid Ack # %d with checksum %d \n", packet.acknum,packet.checksum);
            corruptAck++;
        }
        else
        {
            ackWithInvalidSeqNum++;
        }
    }
}//method ends

/* called when A's timer goes off */
void A_timerinterrupt()
{
    /*compare time elapsed since each packet was sent with the timeout*/
    //printf("-------------inside timerinterrupt---------------------\n");
    int y=baseA;
    slidingWindow=baseA+windowSize;
    while(y<slidingWindow)
    {
        if(ptrA[y].pushedtToL3==true)
        {
            if(ptrA[y].pushedtToL5==false)
            {
                float currentTime=get_sim_time();
                timeElapsed=currentTime-ptrA[y].sendingTime;
                if(timeElapsed>=timerIncrement)
                {
                    ptrA[y].sendingTime=get_sim_time();
                    tolayer3(0, ptrA[y].packet);
                    //printf("packet #%d retransmitted\n", ptrA[y].packet.seqnum);
                    
                    if(at%5==0)
                    {
                        adaptiveTimeout();
                    }
                    
                }
            }
        }
        y++;    //increment while
    }
    starttimer(0,1.0);
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
    windowSize=getwinsize();
    //printf("window size selected is: %d\n", windowSize);
    timerIncrement=12.0;
    baseA=1;
    seekerA=1;
    corruptAck=0;
    ackWithInvalidSeqNum=0;
    corruptPacket=0;
    starttimer(0,1.0);
    denominator=0;
    at=0;
    
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
    printf("--------------------Inside B_input---------------------\n");
    //setToFalse(repeatedPacket);
    
    int verifyChecksum=0;
    int payloadChecksum=0;
    verifyChecksum=packet.seqnum+packet.acknum;
    
    int i=0;
    while(i<(sizeof(packet.payload)))
    {
        payloadChecksum=payloadChecksum+packet.payload[i];
        i++;
    }
    verifyChecksum=(verifyChecksum+payloadChecksum)*2;
    //printf("@@@@@@@@@@@@@@@ Sent from A : %d\n",pack.checksum);
    //printf("@@@@@@@@@@@@@@@ Received at B : %d\n",verifyChecksum);
    
    int goodOrBad=isCorrupt(packet.checksum,verifyChecksum);
    if(goodOrBad==0)
    {
        //printf("bad packet\n");
        corruptPacket++;
        return;
    }
    
    repeatedPacket=false;
    
    if(ptrB[packet.seqnum].partOfBuffer==true)
    {
        if(ptrB[packet.seqnum].packet.seqnum==packet.seqnum)
        {
            //setToTrue(repeatedPacket);
            repeatedPacket=setToTrue(repeatedPacket);
        }
    }
    
    slidingWindow=baseB+windowSize;
    if(goodOrBad==1)
    {
        //printf("good packet moving forward\n");
        if(repeatedPacket==false)
        {
            //printf("packet not duplicate moving forward\n");
            //printf("pacekt.seqnum value is: %d\n", packet.seqnum);
            //printf("base B value is: %d\n", baseB);
            if(packet.seqnum>=baseB)
            {
                if(packet.seqnum<slidingWindow)
                {
                    //printf("packet %d falls in sliding window\n",packet.seqnum);
                    /*control will reach here if B reveives a valid packet
                     
                     Next steps would be:
                     STEP 1. Prepare ack (packetize) and send to A
                     STEP 2. update seeker postion in buffer
                     STEP 3. buffer packet at that position
                     STEP 4. check if the packet can be sent to L7?
                     
                    */
                    
                    //STEP 1
                    setToZero(ackFromB.checksum);
                    ackFromB.acknum=packet.seqnum;
                    ackFromB.seqnum=packet.seqnum;
                    strncpy(ackFromB.payload, "acknowledgement",sizeof(ackFromB.payload));
                    ackFromB.checksum=calculateChecksum(ackFromB);
                    tolayer3(1,ackFromB);
                    //printf("ack #%d with checksum %d sent from B sent\n",ackFromB.acknum,ackFromB.checksum);
                    
                    //STEP 2
                    seekerB=packet.seqnum;
                    
                    //STEP 3
                    ptrB[seekerB].packet=packet;
                    ptrB[seekerB].partOfBuffer=setToTrue(ptrB[seekerB].partOfBuffer);
                    
                    //STEP 4
                    int g=baseB;
                    while(g<slidingWindow)
                    {
                        if(ptrB[g].partOfBuffer==true)
                        {
                            tolayer5(1,ptrB[g].packet.payload);
                            //printf("##############packet %d received at L5\n", ptrB[g].packet.seqnum);
                            baseB++;
                        }
                        else
                        {
                            break;
                        }
                        g++; //increment while
                    }
            
                }
            }
        }
    }
  
    
        if(goodOrBad==1 && repeatedPacket==true)
        {
            setToZero(ackFromB.checksum);
            ackFromB.acknum=packet.seqnum;
            ackFromB.seqnum=packet.seqnum;
            strncpy(ackFromB.payload, "acknowledgement",sizeof(ackFromB.payload));
            ackFromB.checksum=calculateChecksum(ackFromB);
            tolayer3(1,ackFromB);
           //printf("ack # %d from B sent for duplicate packet\n",ackFromB.acknum);
            return;
        }
    //if of goodOrBad ends
    
    //printf("--------------------Exiting B_input---------------------\n");
}// method ends

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
    baseB=1;
    seekerB=0;
}

/*****************************************************************************/

int win_size;

int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msgs to generate, then stop */

float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
    double mmm = 2147483647;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
    float x;                   /* individual students may need to change mmm */
    x = rand()/mmm;            /* x should be uniform in [0,1] */
    return(x);
}


/*****************************************************************
 ***************** NETWORK EMULATION CODE IS BELOW ***********
 The code below emulates the layer 3 and below network environment:
 - emulates the tranmission and delivery (possibly with bit-level corruption
 and packet loss) of packets across the layer 3/4 interface
 - handles the starting/stopping of a timer, and generates timer
 interrupts (resulting in calling students timer handler).
 - generates message to be sent (passed from later 5 to 4)
 
 THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
 THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
 OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
 the emulator, you're welcome to look at the code - but again, you should have
 to, and you defeinitely should not have to modify
 ******************************************************************/



/* possible events: */
#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1


struct event {
    float evtime;           /* event time */
    int evtype;             /* event type code */
    int eventity;           /* entity where event occurs */
    struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
    struct event *prev;
    struct event *next;
};
struct event *evlist = NULL;   /* the event list */


void insertevent(struct event *p)
{
    struct event *q,*qold;
    
    if (TRACE>2) {
        printf("            INSERTEVENT: time is %lf\n",time_local);
        printf("            INSERTEVENT: future time will be %lf\n",p->evtime);
    }
    q = evlist;     /* q points to header of list in which p struct inserted */
    if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
    }
    else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
            qold=q;
        if (q==NULL) {   /* end of list */
            qold->next = p;
            p->prev = qold;
            p->next = NULL;
        }
        else if (q==evlist) { /* front of list */
            p->next=evlist;
            p->prev=NULL;
            p->next->prev=p;
            evlist = p;
        }
        else {     /* middle of list */
            p->next=q;
            p->prev=q->prev;
            q->prev->next=p;
            q->prev=p;
        }
    }
}


/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

void generate_next_arrival()
{
    double x,log(),ceil();
    struct event *evptr;
    //    //char *malloc();
    float ttime;
    int tempint;
    
    if (TRACE>2)
        printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
    
    x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
    /* having mean of lambda        */
    
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime =  time_local + x;
    evptr->evtype =  FROM_LAYER5;
    if (BIDIRECTIONAL && (jimsrand()>0.5) )
        evptr->eventity = B;
    else
        evptr->eventity = A;
    insertevent(evptr);
}





void init(int seed)                         /* initialize the simulator */
{
    int i;
    float sum, avg;
    float jimsrand();
    
    /*
     printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
     printf("Enter the number of messages to simulate: ");
     scanf("%d",&nsimmax);
     printf("Enter  packet loss probability [enter 0.0 for no loss]:");
     scanf("%f",&lossprob);
     printf("Enter packet corruption probability [0.0 for no corruption]:");
     scanf("%f",&corruptprob);
     printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
     scanf("%f",&lambda);
     printf("Enter TRACE:");
     scanf("%d",&TRACE);
     */
    
    srand(seed);              /* init random number generator */
    sum = 0.0;                /* test random number generator for students */
    for (i=0; i<1000; i++)
        sum=sum+jimsrand();    /* jimsrand() should be uniform in [0,1] */
    avg = sum/1000.0;
    if (avg < 0.25 || avg > 0.75) {
        printf("It is likely that random number generation on your machine\n" );
        printf("is different from what this emulator expects.  Please take\n");
        printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
        exit(0);
    }
    
    ntolayer3 = 0;
    nlost = 0;
    ncorrupt = 0;
    
    time_local=0;                    /* initialize time to 0.0 */
    generate_next_arrival();     /* initialize event list */
}






//int TRACE = 1;             /* for my debugging */
//int nsim = 0;              /* number of messages from 5 to 4 so far */
//int nsimmax = 0;           /* number of msgs to generate, then stop */
//float time = 0.000;
//float lossprob;            /* probability that a packet is dropped  */
//float corruptprob;         /* probability that one bit is packet is flipped */
//float lambda;              /* arrival rate of messages from layer 5 */
//int   ntolayer3;           /* number sent into layer 3 */
//int   nlost;               /* number lost in media */
//int ncorrupt;              /* number corrupted by media*/

/**
 * Checks if the array pointed to by input holds a valid number.
 *
 * @param  input char* to the array holding the value.
 * @return TRUE or FALSE
 */
int isNumber(char *input)
{
    while (*input){
        if (!isdigit(*input))
            return 0;
        else
            input += 1;
    }
    
    return 1;
}

int read_arg_int(char c)
{
    if(!isNumber(optarg)) {
        fprintf(stderr, "Invalid value for -%c\n", c);
        exit(-1);
    }
    return atoi(optarg);
}

float read_arg_float(char c)
{
    float val = atof(optarg);
    if(val < 0.0 || val > 1.0){
        fprintf(stderr, "Invalid value for -%c\n", c);
        exit(-1);
    }
    return val;
}

void display_usage(char *filename)
{
    printf("Usage:\n %s -s Seed -w Window size -m Number of messages to simulate -l Loss -c Corruption -t Average time between messages from sender's layer5 -v Tracing\n", filename);
}

int main(int argc, char **argv)
{
    struct event *eventptr;
    struct msg  msg2give;
    struct pkt  pkt2give;
    
    int i,j;
    char c;
    
    int opt;
    int seed;
    
    //Check for number of arguments
    if(argc != 15){
        fprintf(stderr, "Missing arguments!\n");
        display_usage(argv[0]);
        return -1;
    }
    
    /*
     * Parse the arguments
     * http://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html
     */
    while((opt = getopt(argc, argv,"s:w:m:l:c:t:v:")) != -1){
        switch (opt){
            case 's':   seed = read_arg_int(opt);
                break;
            case 'w':   win_size = read_arg_int(opt);
                break;
            case 'm': 	nsimmax = read_arg_int(opt);
                break;
            case 'l': 	lossprob = read_arg_float(opt);
                break;
            case 'c': 	corruptprob = read_arg_float(opt);
                break;
            case 't': 	if((lambda = atof(optarg)) <= 0.0){
            				fprintf(stderr, "Invalid value for -%c\n", opt);
                exit(-1);
            }
                break;
            case 'v': 	TRACE = read_arg_int(opt);
                break;
            case '?':
           	default:    fprintf(stderr, "Invalid arguments!\n");
                display_usage(argv[0]);
                return -1;
        }
    }
    
    init(seed);
    A_init();
    B_init();
    
    while (1) {
        eventptr = evlist;            /* get next event to simulate */
        if (eventptr==NULL)
            goto terminate;
        evlist = evlist->next;        /* remove this event from event list */
        if (evlist!=NULL)
            evlist->prev=NULL;
        if (TRACE>=2) {
            printf("\nEVENT time: %f,",eventptr->evtime);
            printf("  type: %d",eventptr->evtype);
            if (eventptr->evtype==0)
                printf(", timerinterrupt  ");
            else if (eventptr->evtype==1)
                printf(", fromlayer5 ");
            else
                printf(", fromlayer3 ");
            printf(" entity: %d\n",eventptr->eventity);
        }
        time_local = eventptr->evtime;        /* update time to next event time */
        if (nsim==nsimmax)
            break;                        /* all done with simulation */
        if (eventptr->evtype == FROM_LAYER5 ) {
            generate_next_arrival();   /* set up future arrival */
            /* fill in msg to give with string of same letter */
            j = nsim % 26;
            for (i=0; i<20; i++)
                msg2give.data[i] = 97 + j;
            if (TRACE>2) {
                printf("          MAINLOOP: data given to student: ");
                for (i=0; i<20; i++)
                    printf("%c", msg2give.data[i]);
                printf("\n");
            }
            nsim++;
            if (eventptr->eventity == A)
            {
                A_application += 1;
                A_output(msg2give);
            }
            /*
             else
             B_output(msg2give);
             */
        }
        else if (eventptr->evtype ==  FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i=0; i<20; i++)
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
            if (eventptr->eventity ==A)      /* deliver packet by calling */
                A_input(pkt2give);            /* appropriate entity */
            else
            {
                B_transport += 1;
                B_input(pkt2give);
            }
            free(eventptr->pktptr);          /* free the memory for packet */
        }
        else if (eventptr->evtype ==  TIMER_INTERRUPT) {
            if (eventptr->eventity == A)
                A_timerinterrupt();
            /*
             else
             B_timerinterrupt();
             */
        }
        else  {
            printf("INTERNAL PANIC: unknown event type \n");
        }
        free(eventptr);
    }
    
terminate:
    //Do NOT change any of the following printfs
    printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",time_local,nsim);
    
    printf("\n");
    printf("[PA2]%d packets sent from the Application Layer of Sender A[/PA2]\n", A_application);
    printf("[PA2]%d packets sent from the Transport Layer of Sender A[/PA2]\n", A_transport);
    printf("[PA2]%d packets received at the Transport layer of Receiver B[/PA2]\n", B_transport);
    printf("[PA2]%d packets received at the Application layer of Receiver B[/PA2]\n", B_application);
    //printf("# %d packets with INVALID SEQUENCE NUMBERS AT B\n",packetWithInvalidSeqNum);//added by agautam2 REMOVE LATER
    printf("# %d ACKs with INVALID SEQUENCE NUMBERS AT A\n",ackWithInvalidSeqNum);      //added by agautam2 REMOVE LATER
    printf("# %d Courrupt Packets RCVD at B\n",corruptPacket);                                   //added by agautam2 REMOVE LATER
    printf("# %d Courrupt ACKs RCVD at A\n",corruptAck);                                         //added by agautam2 REMOVE LATER
    printf("[PA2]Total time: %f time units[/PA2]\n", time_local);
    printf("[PA2]Throughput: %f packets/time units[/PA2]\n", B_application/time_local);
    
    printf("[agautam2]%d acks were received at A[/agautam2]\n",ackCount);               //added by agautam2 REMOVE LATER
    float efficiency=0.0;
    efficiency=(float)ackCount/A_application;
    printf("[agautam2]Efficiency of SR is %f [/agautam2]\n",efficiency);               //added by agautam2 REMOVE LATER

    
    return 0;
}


/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

/*void generate_next_arrival()
 {
 double x,log(),ceil();
 struct event *evptr;
 //char *malloc();
 float ttime;
 int tempint;
 
 if (TRACE>2)
 printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
 
 x = lambda*jimsrand()*2;  // x is uniform on [0,2*lambda]
 // having mean of lambda
 evptr = (struct event *)malloc(sizeof(struct event));
 evptr->evtime =  time + x;
 evptr->evtype =  FROM_LAYER5;
 if (BIDIRECTIONAL && (jimsrand()>0.5) )
 evptr->eventity = B;
 else
 evptr->eventity = A;
 insertevent(evptr);
 } */




void printevlist()
{
    struct event *q;
    int i;
    printf("--------------\nEvent List Follows:\n");
    for(q = evlist; q!=NULL; q=q->next) {
        printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
    printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB)
//AorB;  /* A or B is trying to stop timer */
{
    struct event *q,*qold;
    
    if (TRACE>2)
        printf("          STOP TIMER: stopping timer at %f\n",time_local);
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q=evlist; q!=NULL ; q = q->next)
        if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
            /* remove this event */
            if (q->next==NULL && q->prev==NULL)
                evlist=NULL;         /* remove first and only event on list */
            else if (q->next==NULL) /* end of list - there is one in front */
                q->prev->next = NULL;
            else if (q==evlist) { /* front of list - there must be event after */
                q->next->prev=NULL;
                evlist = q->next;
            }
            else {     /* middle of list */
                q->next->prev = q->prev;
                q->prev->next =  q->next;
            }
            free(q);
            return;
        }
    printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


void starttimer(int AorB,float increment)
// AorB;  /* A or B is trying to stop timer */

{
    
    struct event *q;
    struct event *evptr;
    ////char *malloc();
    
    if (TRACE>2)
        printf("          START TIMER: starting timer at %f\n",time_local);
    /* be nice: check to see if timer is already started, if so, then  warn */
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q=evlist; q!=NULL ; q = q->next)
        if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
            printf("Warning: attempt to start a timer that is already started\n");
            return;
        }
    
    /* create future event for when timer goes off */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime =  time_local + increment;
    evptr->evtype =  TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
}


/************************** TOLAYER3 ***************/
void tolayer3(int AorB,struct pkt packet)
{
    struct pkt *mypktptr;
    struct event *evptr,*q;
    ////char *malloc();
    float lastime, x, jimsrand();
    int i;
    
    
    ntolayer3++;
    
    if(AorB == 0) A_transport += 1;
    
    /* simulate losses: */
    if (jimsrand() < lossprob)  {
        nlost++;
        if (TRACE>0)
            printf("          TOLAYER3: packet being lost\n");        //added by agautam2 REMOVE LATER
            return;
    }
    
    /* make a copy of the packet student just gave me since he/she may decide */
    /* to do something with the packet after we return back to him/her */
    mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
    mypktptr->seqnum = packet.seqnum;
    mypktptr->acknum = packet.acknum;
    mypktptr->checksum = packet.checksum;
    for (i=0; i<20; i++)
        mypktptr->payload[i] = packet.payload[i];
    if (TRACE>2)  {
        printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
               mypktptr->acknum,  mypktptr->checksum);
        for (i=0; i<20; i++)
            printf("%c",mypktptr->payload[i]);
        printf("\n");
    }
    
    /* create future event for arrival of packet at the other side */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
    evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
    evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
    /* finally, compute the arrival time of packet at the other end.
     medium can not reorder, so make sure packet arrives between 1 and 10
     time units after the latest arrival time of packets
     currently in the medium on their way to the destination */
    lastime = time_local;
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
    for (q=evlist; q!=NULL ; q = q->next)
        if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) )
            lastime = q->evtime;
    evptr->evtime =  lastime + 1 + 9*jimsrand();
    
    
    
    /* simulate corruption: */
    if (jimsrand() < corruptprob)
    {
        ncorrupt++;
        if ( (x = jimsrand()) < .75)
            mypktptr->payload[0]='Z';   /* corrupt payload */
        else if (x < .875)
            mypktptr->seqnum = 999999;
        else
            mypktptr->acknum = 999999;
        if (TRACE>0)
        {}
        //printf("          TOLAYER3: packet being corrupted\n");       //Added by agautam2 REMOVE LATER
    }
    
    if (TRACE>2)
        printf("          TOLAYER3: scheduling arrival on other side\n");
    insertevent(evptr);
}

void tolayer5(int AorB,char *datasent)
{
    
    int i;
    if (TRACE>2) {
        printf("          TOLAYER5: data received: ");
        for (i=0; i<20; i++)
            printf("%c",datasent[i]);
        printf("\n");
    }
    if(AorB == 1) B_application += 1;
}

int getwinsize()
{
    return win_size;
}

float get_sim_time()
{
    return time_local;
}


