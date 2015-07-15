////////////////////////////////////////////////////////////////////////////
//
// apicvt.c                        I    OOO                           h
//                                 I   O   O     t      eee    cccc   h
//                                 I  O     O  ttttt   e   e  c       h hhh
// -----------------------------   I  O     O    t     eeeee  c       hh   h
// copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
//    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
//
////////////////////////////////////////////////////////////////////////////
//
//   Copyright (C) 86, 91, 1995-2002, Free Software Foundation, Inc.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2, or (at your option)
//   any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software Foundation,
//   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
//
////////////////////////////////////////////////////////////////////////////
//
// Thanks to Amish S. Dave for getting us started up!
// Savagely hacked by Paul Knowles <Paul.Knowles@unifr.ch>,  May, 2003
//
// Many Thanks Paul, MAH
//
////////////////////////////////////////////////////////////////////////////
//
// Please email liunx@iotech with any bug reports questions or comments
//
////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdio.h>
#include "iottypes.h"

#include "daqx.h"
#include "cmnapi.h"
#include "api.h"
    
   
    typedef struct {
      unsigned       nscan;
      unsigned       readingsPos;
      unsigned       nReadings;
      float          signal1;
      float          voltage1;
      float          signal2;
      float          voltage2;
      unsigned       avg;
    } LinearSetupT;
    
      typedef struct {
       DWORD       nscan;
       DWORD       zeroPos;
       DWORD       readingsPos;
       DWORD       nReadings;
    } ZeroSetupT;
    
    typedef struct {
       BOOL  zeroDbk19;
    } ApiCvtT;
    
    
    static ApiCvtT    session[MaxSessions];
    static LinearSetupT LinearSetup;
    static ZeroSetupT  zeroSetup;
    float Admin,Admax;
   
    VOID
    apiCvtMessageHandler(MsgPT msg)
    { 
    
       if (msg->errCode == DerrNoError)
          {
          switch ( msg->msg )
             {
             case MsgAttach:
                
    
                break; 
    
             case MsgDetach:
                
    
                break; 
    

             case MsgInit:
               
                session[msg->handle].zeroDbk19 = FALSE;
			
                       
                  if (Is10V(msg->deviceType))
                  {
                     Admin = -10.0f;
                     Admax = 10.0f;
                  }
                  else
                  {
                     Admin = -5.0f;
                     Admax = 5.0f;
                  }
                break; 
    
             case MsgClose:
                
    
                break; 
             }
          }
    
      
    } 
    
 
   #define OEC 
   
   #define TC_OVERRANGE_VAL 0x8000 
    
    
	
	#define  INCLUDE_NBS_T2V_DATA 
  
    
    
    typedef struct {
       float   startTemp,  
               endTemp;
       int     ncoeff;     
       const double  *coeff;     
       #ifdef   INCLUDE_NBS_T2V_DATA
       int     expTerm;   
       #endif   
       } T2VT;
    
    
    typedef struct {
       float          startVolt,  
                      endVolt;
       int            ncoeff;    
       const double   *coeff;    
       } V2TT;
    
    typedef struct {
       char           *tcName;    
       double         uniGain;     
       double         biGain;       
       #ifdef   INCLUDE_NBS_T2V_DATA
       int            nt2v;       
       const T2VT           *t2v;      
       #endif   
       int            ncjc;      
       const double   *cjc;       
       int            nv2t;       
       const V2TT     *v2t;       
       } ThermocoupleT;
    
    typedef struct {
       DWORD               nscan;
       DWORD               cjcPosition;
       DWORD               ntc;
       const ThermocoupleT *tcType;
       DaqTCSetupHwType    hwType;
       DWORD               avg;
       BOOL                bipolar;
    } TCSetupT;
    
    typedef struct {
       DWORD nReadings;
       DWORD startPosition;
       DWORD nRtd;
       DWORD rtdValue;
       DWORD avg;
    } RtdSetupT;

    #define LOW_NIBL(byte)        (byte & 0x0F)
    #define UP_NIBL(byte)         ((byte & 0xF0)>>4)
    
    #define  LENGTH(a)   (sizeof(a)/sizeof(a[0]))
    
   
    #define BINARY(b) (1<<(b))
    #define DBK14DECADE(d) ( (d)<2 ?((d)<1 ?1 :10) : ((d)<3 ?100 :1000) )
    #define DBK19DECADE(d) ( (d)<2 ?((d)<1 ?60 :90) : ((d)<3 ?180 :240) )
    #define DBK14GAIN(code) (1.0f / (BINARY(code>>2) * DBK14DECADE(code&0x3)) )
    #define DBK19GAIN(code) (1.0f / (BINARY(code>>2) * DBK19DECADE(code&0x3)) )

    
    #define DBK81DECADE(d)  ( 99.915f )    
    #define DBK90DECADE(d)  ( 99.915f )    
    #define DBK81GAIN(code) (1.0f / (BINARY(code>>2) * DBK81DECADE(code&0x3)) )
    #define DBK90GAIN(code) (1.0f / (BINARY(code>>2) * DBK90DECADE(code&0x3)) )

    
    
    #define RtdCurveOffset 0   
    #define RtdCurveSlope 1    
    #define RtdCurveValues 2
    
    typedef  struct {
       double   value;
    } CoeffT,VoltageT,TempT;
    
    
    
    static double result;
    
    static const int RtdPt385[] =
       {
       //interval 0
       -15676,  572,  -15104,  581,  -14524,  589,  -13935,  596,  -13339,  603, 
       -12736,  610,  -12126,  616,  -11511,  622,  -10889,  627,  -10262,  632, 
       -9630,   636,  -8994,   641,  -8353,   644,  -7708,   648,  -7060,   651, 
       -6409,   655,  -5754,   658,  -5096,   661,  -4435,   664,  -3771,   668, 
       -3103,   671,  -2432,   675,  -1757,   678,  -1079,   682,  -397,    685,
       288,     689,  977,     693,  1669,    697,  2366,    700,  3067,    704, 
       3771,    708,  4479,    712,  5192,    716,  5908,    721,  6628,    725,
       7354,    729,  8083,    734,  8817,    738,  9555,    743,  10298,   748, 
       11045,   752,  11798,   757,  12555,   762,  13317,   767,  14085,   772, 
       14857,   778,  15635,   783,  16417,   789,  17206,   794,  18000,   800, 
       18800,   806,  19606,   812,  20418,   818,  21236,   824,  22060,   831, 
       22891,   837,  23728,   844,  24572,   851,  25423,   858,  26281,   865, 
       27146,   873,  28018,   880,  28899,   888,  29787,   896                 
       };
    
    #ifdef   INCLUDE_NBS_T2V_DATA
 
    static const double t2vK1[] =
       {
       0.,                  39.475433139E-06,        0.027465251138E-06,
       -.00016565406716E-06,    -.0000015190912392E-06,   -2.4581670924E-14,
       -2.4757917816E-16,   -1.5585276173E-18,   -5.9729921255E-21,
       -1.2688801216E-23,   -1.1382797374E-26
       };
    
  

    
    static const double t2vK2[] =
       {
       -18.533063273E-06,       38.918344612E-06,        .016645154356E-06,
       -.000078702374448E-06,   2.2835785557E-13,    -3.5700231258E-16,
       2.9932909136E-19,    -1.2849848798E-22,   2.2239974336E-26
       };
    
    static const T2VT t2vK[] =
       {
    //   {  -270., 0.,     // Temperature range
    //      LENGTH(t2vK1), t2vK1,     // Equation Elements
    //      0
    //   },
       {  -200.0f, 0.0f,     // Temperature range
          LENGTH(t2vK1), t2vK1,     // Equation Elements
          0
       },
       {  0.0f, 1372.0f,     // Temperature range
          LENGTH(t2vK2), t2vK2,     // Equation Elements
          1
       }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcK[] =
       {
          3.946980079452559E-05,
          2.306321511889856E-08,
         -5.651663053941205E-11,
         -2.585531739489604E-13
       };
    
    static const double v2tK1[] =
       {
               24.6929978381369,
             -2.201677335985231,
             -1.488734813041724,
            -0.6124498618723279,
             -0.108617861060782,
          -0.007546430483816082
       };
    
    static const double v2tK2[] =
       {
               25.4157902305133,
            -0.2741457709914348,
            -0.1231188185083917,
            0.06532785761440052,
           -0.01229108928682574,
           0.001293889156413027,
         -8.703977256503159E-05,
          3.964606511861641E-06,
         -1.253371712008026E-07,
          2.758332385422134E-09,
         -4.150726874521282E-11,
          4.075290327860387E-13,
         -2.353513959301694E-15,
          6.065466920481257E-18
       };
    
    static const V2TT v2tK[] =
       {
       {   -0.00589136f,            0.0f,   // Voltage Range
          LENGTH(v2tK1), v2tK1      // Equation Elements
       },
       {             0.0f,    0.0548749f,   // Voltage Range
          LENGTH(v2tK2), v2tK2      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeK =
       {
       "K",
       DBK14GAIN(Dbk14UniTypeK), DBK14GAIN(Dbk14BiTypeK),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vK), t2vK,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcK), cjcK,
       LENGTH(v2tK), v2tK
       };
    
    static const ThermocoupleT dbk19typeK =
       {
       "K",
       DBK19GAIN(Dbk19UniTypeK), DBK19GAIN(Dbk19BiTypeK),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vK), t2vK,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcK), cjcK,
       LENGTH(v2tK), v2tK
       };
    
    static const ThermocoupleT tbktypeK =
      {
      "K",
      1.0/200.0, 1.0/100.0,
      #ifdef   INCLUDE_NBS_T2V_DATA
      LENGTH(t2vK), t2vK,
      #endif   // INCLUDE_NBS_T2V_DATA
      LENGTH(cjcK), cjcK,
      LENGTH(v2tK), v2tK
      };

    // DBK81/82
    static const ThermocoupleT dbk81typeK =
       {
       "K",
       DBK81GAIN(Dbk81TypeK), DBK81GAIN(Dbk81TypeK), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vK), t2vK,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcK), cjcK,
       LENGTH(v2tK), v2tK
       };
    
    // DBK90
    static const ThermocoupleT dbk90typeK =
       {
       "K",
       DBK90GAIN(Dbk90TypeK), DBK90GAIN(Dbk90TypeK), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vK), t2vK,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcK), cjcK,
       LENGTH(v2tK), v2tK
       };

     #ifdef   INCLUDE_NBS_T2V_DATA
     // Type T Coefficients - -270C thru 0C
     // Ref: NBS Monograph 125
     // 3/74 Page 183

     
     static const double   t2vT1[] =
       {
       0.,                  38.74077384E-06,         .044123932482E-06,
       .00011405238498E-06,     .000019974406568E-06,    9.0445401187E-13,
       2.2766018504E-14,    3.624740938E-16,     3.8648924201E-18,
       2.8298678519E-20,    1.4281383349E-22,    4.8833254364E-25,
       1.0803474683E-27,    1.3949291026E-30,    7.9795893156E-34
       };
    
    // Type T Coefficients - 0C thru 400C
    // Ref: NBS Monograph 125
    // 3/74 Page 183
    
    static const double   t2vT2[] =
       {
       0.,                  38.74077384E-06,         .033190198092E-06,
       .00020714183645E-06,     -.0000021945834823E-06,  1.103190055E-14,
       -3.0927581898E-17,   4.5653337165E-20,    -2.761687804E-23
       };
    
    static const T2VT t2vT[] =
       {
    //   {  -270., 0.,     // Temperature range
    //      LENGTH(t2vT1), t2vT1,     // Equation Elements
    //      0
    //   },
       {  -200.0f, 0.0f,     // Temperature range
          LENGTH(t2vT1), t2vT1,     // Equation Elements
          0
       },
       {  0.0f, 400.0f,      // Temperature range
          LENGTH(t2vT2), t2vT2,     // Equation Elements
          0

       }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcT[] =
       {
          3.873638477497319E-05,
          3.383460902066653E-08,
          1.761385688528543E-10,
         -1.529938223576803E-12,
           4.22234443348533E-15
       };
    

    static const double v2tT1[] =
       {
              26.41416700156108,
             0.4895905196629103,
             0.8940199613963158,
             0.2101339029460585,
            0.02121092664263423
       };
    
    static const double v2tT2[] =
       {
              25.86521969540766,
            -0.7145446038203612,
            0.03620773516208204,
          -0.001174435290023805,
          1.645158252047959E-05
    
          };
    
    static const V2TT v2tT[] =
       {
       {   -0.00560292f,            0.0f,   // Voltage Range
          LENGTH(v2tT1), v2tT1      // Equation Elements
       },
       {             0.0f,    0.0208692f,   // Voltage Range
          LENGTH(v2tT2), v2tT2      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeT =
       {
       "T",
       DBK14GAIN(Dbk14UniTypeT), DBK14GAIN(Dbk14BiTypeT),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vT), t2vT,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcT), cjcT,
       LENGTH(v2tT), v2tT
       };
    
    static const ThermocoupleT dbk19typeT =
       {
       "T",
       DBK19GAIN(Dbk19UniTypeT), DBK19GAIN(Dbk19BiTypeT),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vT), t2vT,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcT), cjcT,
       LENGTH(v2tT), v2tT
       };
    
   static const ThermocoupleT tbktypeT =
      {
      "T",
      1.0/200.0, 1.0/200.0,
      #ifdef   INCLUDE_NBS_T2V_DATA
      LENGTH(t2vT), t2vT,
      #endif   // INCLUDE_NBS_T2V_DATA
      LENGTH(cjcT), cjcT,
      LENGTH(v2tT), v2tT
      };

    // DBK81/82
    static const ThermocoupleT dbk81typeT =
       {
       "T",
       DBK81GAIN(Dbk81TypeT), DBK81GAIN(Dbk81TypeT), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vT), t2vT,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcT), cjcT,
       LENGTH(v2tT), v2tT
       };

  // DBK90
    static const ThermocoupleT dbk90typeT =
       {
       "T",
       DBK90GAIN(Dbk90TypeT), DBK90GAIN(Dbk90TypeT), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vT), t2vT,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcT), cjcT,
       LENGTH(v2tT), v2tT
       };



    #ifdef   INCLUDE_NBS_T2V_DATA
    // Type E Coefficients - -270C thru 0C
    // Ref: NBS Monograph 125
    // 3/74 Page 93
    
    static const double   t2vE1[] =
       {
       0.,                  58.695857799E-06,        .051667517705E-06,
       -.00044652683347E-06,    -.000017346270905E-06,   -4.8719368427E-13,
       -8.8896550447E-15,   -1.0930767375E-16,   -9.1784535039E-19,
       -5.2575158521E-21,   -2.0169601996E-23,   -4.9502138782E-26,
       -7.0177980633E-29,   -4.3671808488E-32
       };
    
    // Type E Coefficients - 0C thru 1000C
    // Ref: NBS Monograph 125
    // 3/74 Page 93
    
    static const double   t2vE2[] =
       {
       0,                   58.695857799E-06,        .043110945462E-06,
       .000057220358202E-06,    -5.4020668085E-13,   1.5425922111E-15,
       -2.4850089136E-18,   2.3389721459E-21,    -1.1946296815E-24,
       2.5561127497E-28
       };
    
    static const T2VT t2vE[] =
       {
       {  -270.0f, 0.0f,     // Temperature range
          LENGTH(t2vE1), t2vE1,     // Equation Elements
          0
       },
       {  0.0f, 1000.0f,     // Temperature range
          LENGTH(t2vE2), t2vE2,     // Equation Elements
          0
       }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcE[] =
       {
          5.868911066109877E-05,
          4.379427197485462E-08,
           3.53510489031258E-11,
         -2.516737123756666E-13
       };
    
    static const double v2tE1[] =
       {
              853585051.2828077,
              348727873.9951037,
              53426630.24944823,
              3637863.349626207,
              92889.42205790414
       };
    
    static const double v2tE2[] =
       {
              19.60518963900145,
              24.07470718853289,
              80.79382604502418,
              135.4817626096547,
              134.7461768486186,
              86.74360166475027,
              38.09337891214098,
              11.77797013865053,
              2.607469201082481,
             0.4151753036534207,
            0.04715152724234191,
           0.003726222617876621,
          0.0001946650747591956,
          6.042759406129798E-06,
          8.438766586359822E-08
       };
    
    static const double v2tE3[] =
       {
              17.05783289597932,
            -0.2357329828463777,
           0.007357464137651615,
         -0.0001543373464364444,
          2.057907763567549E-06,
         -1.519790177141258E-08,
          4.714692394742069E-11
       };
    
    static const V2TT v2tE[] =
       {
       {   -0.00983503f,  -0.00976225f,   // Voltage Range
          LENGTH(v2tE1), v2tE1      // Equation Elements
       },
       {   -0.00976225f,            0.0f,   // Voltage Range
          LENGTH(v2tE2), v2tE2      // Equation Elements
       },
       {             0.0f,    0.0763575f,   // Voltage Range
          LENGTH(v2tE3), v2tE3      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeE =
       {
       "E",
       DBK14GAIN(Dbk14UniTypeE), DBK14GAIN(Dbk14BiTypeE),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vE), t2vE,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcE), cjcE,
       LENGTH(v2tE), v2tE
       };
    
    static const ThermocoupleT dbk19typeE =
       {
       "E",
       DBK19GAIN(Dbk19UniTypeE), DBK19GAIN(Dbk19BiTypeE),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vE), t2vE,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcE), cjcE,
       LENGTH(v2tE), v2tE
       };

    static const ThermocoupleT tbktypeE =
       {
       "E",
       1.0/100.0, 1.0/50.0,
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vE), t2vE,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcE), cjcE,
       LENGTH(v2tE), v2tE
       };

    // DBK81/82
    static const ThermocoupleT dbk81typeE =
       {
       "E",
       DBK81GAIN(Dbk81TypeE), DBK81GAIN(Dbk81TypeE), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vE), t2vE,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcE), cjcE,
       LENGTH(v2tE), v2tE
       };

   // DBK90
    static const ThermocoupleT dbk90typeE =
       {


       "E",
       DBK90GAIN(Dbk90TypeE), DBK90GAIN(Dbk90TypeE), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vE), t2vE,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcE), cjcE,
       LENGTH(v2tE), v2tE
       };
   
    #ifdef   INCLUDE_NBS_T2V_DATA
    // Type J Coefficients - -210C thru 760C
    // Ref: NBS Monograph 125
    // 3/74 Page 107
    
    static const double   t2vJ1[] =
       {
       0.,                  50.372753027E-06,        .030425491284E-06,
       -8.5669750464E-11,   1.3348825735E-13,    -1.7022405966E-16,
       1.9416091001E-19,    -9.6391844859E-23
       };
    
    // Type J Coefficients - 760C thru 1200C
    // ***TYPE J NOT STABLE IN THIS RANGE!***
    // Ref: NBS Monograph 125
    // 3/74 Page 107
    
    //static const double   t2vJ2[] =
    //{
    //   2.9721751778E-01,  -1.5059632873E-03,       3.2051064215E-06,
    //   -3.221017423E-09, 1.5949968788E-12,       -3.1239801752E-16
    //};
    
    static const T2VT t2vJ[] =
       {
    //   {  -210., 760.,   // Temperature range
    //      LENGTH(t2vJ1), t2vJ1,     // Equation Elements
    //      0
    //   },
       {  -200.0f, 760.0f,   // Temperature range
          LENGTH(t2vJ1), t2vJ1,     // Equation Elements
          0
       },
    //   {  760., 1200., // Temperature range
    //      LENGTH(t2vJ2), t2vJ2,     // Equation Elements
    //      0
    //   }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcJ[] =
       {
          5.037360187241154E-05,
          3.034054583422683E-08,
         -8.299640805029441E-11,
          9.924951908620768E-14
       };
    
    static const double v2tJ1[] =
       {
              19.78543984700475,
            -0.2360922808563475,
            0.02660179710793189,
          -0.002047045486730421,
          -8.78344400882979E-05,
          2.241424956660385E-05,
           9.14594931233629E-07,
         -4.342202836325123E-07,
          4.304900922215141E-08,
         -2.306015120268235E-09,
           7.66371821428399E-11,
         -1.628525348550859E-12,
          2.159660478074748E-14,
         -1.631174557050024E-16,
          5.361962374854275E-19
       };
    
    static const V2TT v2tJ[] =
       {
       {   -0.00789046f,     0.042922f,   // Voltage Range
          LENGTH(v2tJ1), v2tJ1      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeJ =
       {
       "J",
       DBK14GAIN(Dbk14UniTypeJ), DBK14GAIN(Dbk14BiTypeJ),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vJ), t2vJ,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcJ), cjcJ,
       LENGTH(v2tJ), v2tJ
       };
    
    static const ThermocoupleT dbk19typeJ =
       {
       "J",
       DBK19GAIN(Dbk19UniTypeJ), DBK19GAIN(Dbk19BiTypeJ),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vJ), t2vJ,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcJ), cjcJ,
       LENGTH(v2tJ), v2tJ
       };

   static const ThermocoupleT tbktypeJ =
       {
       "J",
       1.0/200.0, 1.0/100.0,
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vJ), t2vJ,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcJ), cjcJ,
       LENGTH(v2tJ), v2tJ
       };

    // DBK81/82
    static const ThermocoupleT dbk81typeJ =
       {
       "J",
       DBK81GAIN(Dbk81TypeJ), DBK81GAIN(Dbk81TypeJ), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vJ), t2vJ,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcJ), cjcJ,
       LENGTH(v2tJ), v2tJ
       };

// DBK90
    static const ThermocoupleT dbk90typeJ =
       {
       "J",
       DBK90GAIN(Dbk90TypeJ), DBK90GAIN(Dbk90TypeJ), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vJ), t2vJ,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcJ), cjcJ,
       LENGTH(v2tJ), v2tJ
       };
    
    #ifdef   INCLUDE_NBS_T2V_DATA
    // Type N Coefficients AWG 28 - -270C thru 0C
    // Ref: NBS Monograph 161
    // 4/78 Page 49
    
    static const double   t2vN281[] =
       {
       0.,                  26.153540164E-06,        .010933114132E-06,
       -9.391712847E-11,    -5.3592739285E-14,   -2.7406835184E-15,
       -2.3370710645E-17,   -7.825068106E-20,    -9.5885491371E-23
       };
    
    // Type N Coefficients AWG 28 - 0C thru 400C
    // Ref: NBS Monograph 161
    // 4/78 Page 49
    
    static const double   t2vN282[] =
       {
       0.,                  26.153540164E-06,        9.316962696E-09,
       .00013507720863E-06,     -8.5131026625E-13,   2.5853558632E-15,
       -3.9887895408E-18,   2.4633802582E-21
       };
    
    static const T2VT t2vN28[] =
       {
       {  -270.0f, 0.0f, // Temperature range
          LENGTH(t2vN281), t2vN281,     // Equation Elements
          0
       },
       {  0.0f, 400.0f,// Temperature range
          LENGTH(t2vN282), t2vN282,     // Equation Elements
          0
       }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcN28[] =
       {
          2.614229519469767E-05,
          1.045806635912617E-08,
          9.847885874895249E-11,
         -3.674234387824899E-13
       };
    
    static const double v2tN281[] =
       {
              146372325217.3578,
              134928267129.6201,
              46642111422.45473,
              7165903647.635193,
              412852592.4430233
       };
    
    static const double v2tN282[] =
       {
             -11410812146.32164,
             -19264039501.27127,
             -13936445907.86295,
             -5600605959.816684,
             -1350268828.389738,
             -195302538.8090225,
             -15691859.14931726,
             -540276.0432113719
       };
    
    static const double v2tN283[] =
       {
              37.01892383595858,
             -5.506966157084525,
             -6.195716270964484,
             -3.703955189267671,
            -0.9631397717564741,
           -0.09746752538343985
       };
    
    static const double v2tN284[] =
       {
              38.58533945167921,
             -1.084296376302626,
            0.05226308084449349,
          -0.001140568704365572
       };
    
    static const V2TT v2tN28[] =
       {
       {   -0.00434518f,  -0.00433573f,   // Voltage Range
          LENGTH(v2tN281), v2tN281      // Equation Elements
       },
       {   -0.00433573f,  -0.00399036f,   // Voltage Range
          LENGTH(v2tN282), v2tN282      // Equation Elements
       },
       {   -0.00399036f,            0.0f,   // Voltage Range
          LENGTH(v2tN283), v2tN283      // Equation Elements
       },
       {             0.0f,    0.0129755f,   // Voltage Range
          LENGTH(v2tN284), v2tN284      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeN28 =
       {
       "N28",
       DBK14GAIN(Dbk14UniTypeN28), DBK14GAIN(Dbk14BiTypeN28),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN28), t2vN28,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN28), cjcN28,
       LENGTH(v2tN28), v2tN28
       };
    
    static const ThermocoupleT dbk19typeN28 =
       {
       "N28",
       DBK19GAIN(Dbk19UniTypeN28), DBK19GAIN(Dbk19BiTypeN28),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN28), t2vN28,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN28), cjcN28,
       LENGTH(v2tN28), v2tN28
       };

   static const ThermocoupleT tbktypeN28 =
      {
      "N28",
      1.0/200.0, 1.0/100.0,
      #ifdef   INCLUDE_NBS_T2V_DATA
      LENGTH(t2vN28), t2vN28,
      #endif   // INCLUDE_NBS_T2V_DATA
      LENGTH(cjcN28), cjcN28,
      LENGTH(v2tN28), v2tN28
      };

    // DBK81/82
    static const ThermocoupleT dbk81typeN28 =
       {
       "N28",
       DBK81GAIN(Dbk81TypeN28), DBK81GAIN(Dbk81TypeN28), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN28), t2vN28,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN28), cjcN28,
       LENGTH(v2tN28), v2tN28
       };

    // DBK90
    static const ThermocoupleT dbk90typeN28 =
       {
       "N28",
       DBK90GAIN(Dbk90TypeN28), DBK81GAIN(Dbk90TypeN28), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN28), t2vN28,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN28), cjcN28,
       LENGTH(v2tN28), v2tN28
       };

    
    #ifdef   INCLUDE_NBS_T2V_DATA
    // Type N Coefficients AWG 14 - 0C thru 1300C
    // Ref: NBS Monograph 161
    // 4/78 Page 49
    
    static const double   t2vN141[] =
       {
       0.,                  25.897798582E-06,        1.6656127713E-08,
       3.1234962101E-11,    -1.7248130773E-13,   3.652666592E-16,
       -4.4390833504E-19,   3.1553382729E-22,    -1.2150879468E-25,
       1.9557197559E-29
       };
    
    static const T2VT t2vN14[] =
       {
       {  0.0f, 1300.0f, // Temperature range
          LENGTH(t2vN141), t2vN141,     // Equation Elements
          0
       },
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcN14[] =
       {
          2.589599813294714E-05,
          1.683639978137717E-08,
          2.555607685529496E-11,
         -9.958354568692395E-14
       };
    
    static const double v2tN141[] =
       {
              38.70545102257734,
             -1.115558097358063,
            0.06089569833106652,
           -0.00223283525864289,
          5.096645515488165E-05,
         -6.374489023352454E-07,
          3.329280561571737E-09
       };
    
    static const V2TT v2tN14[] =
       {
       {             0.0f,    0.0475018f,   // Voltage Range
          LENGTH(v2tN141), v2tN141      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeN14 =
       {
       "N14",
       DBK14GAIN(Dbk14UniTypeN14), DBK14GAIN(Dbk14BiTypeN14),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN14), t2vN14,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN14), cjcN14,
       LENGTH(v2tN14), v2tN14
       };
    
    static const ThermocoupleT dbk19typeN14 =
       {
       "N14",
       DBK19GAIN(Dbk19UniTypeN14), DBK19GAIN(Dbk19BiTypeN14),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN14), t2vN14,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN14), cjcN14,
       LENGTH(v2tN14), v2tN14
       };

   static const ThermocoupleT tbktypeN14 =
      {
      "N14",
      1.0/200.0, 1.0/100.0,
      #ifdef   INCLUDE_NBS_T2V_DATA
      LENGTH(t2vN14), t2vN14,
      #endif   // INCLUDE_NBS_T2V_DATA
      LENGTH(cjcN14), cjcN14,
      LENGTH(v2tN14), v2tN14
      };

    // DBK81/82
    static const ThermocoupleT dbk81typeN14 =

       {
       "N14",
       DBK81GAIN(Dbk81TypeN14), DBK81GAIN(Dbk81TypeN14), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN14), t2vN14,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN14), cjcN14,
       LENGTH(v2tN14), v2tN14
       };


   // DBK90
    static const ThermocoupleT dbk90typeN14 =
       {
       "N14",
       DBK90GAIN(Dbk90TypeN14), DBK90GAIN(Dbk90TypeN14), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vN14), t2vN14,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcN14), cjcN14,
       LENGTH(v2tN14), v2tN14
       };

    
    #ifdef   INCLUDE_NBS_T2V_DATA
    // Type S Coefficients - -50C thru 630.74C
    // Ref: NBS Monograph 125
    // 3/74 Page 17
    
    static const double   t2vS1[] =
       {
       0.,                  5.3995782346E-06,        .01251977E-06,
       -.000022448217997E-06,   2.8452164949E-14,    -2.2440584544E-17,
       8.5054166936E-21
       };
    
    // Type S Coefficients - 630.74C thru 1064.43C
    // Ref: NBS Monograph 125
    // 3/74 Page 17
    
    static const double   t2vS2[] =
       {
       -298.24481615E-06,       8.2375528221E-06,        .0016453909942E-06
       };
    
    // Type S Coefficients - 1064.43C thru 1665C
    // Ref: NBS Monograph 125
    // 3/74 Page 17
    
    static const double   t2vS3[] =
       {
       1276.6292175E-06,        3.4970908041E-06,        .0063824648666E-06,
       -.0000015722424599E-06
       };
    
    // Type S Coefficients - 1665C thru 1767.6 ***we're going to 1780C!***
    // Ref: NBS Monograph 125
    // 3/74 Page 17
    
    static const double   t2vS4[] =
       {
       97846.655361E-06,        -170.50295632E-06,       .11088699768E-06,
       -.000022494070849E-06
       };
    
    static const T2VT t2vS[] =
       {
       {  -50.0f, 630.74f, // Temperature range
          LENGTH(t2vS1), t2vS1,     // Equation Elements
          0
       },
       {  630.74f, 1064.43f,// Temperature range
          LENGTH(t2vS2), t2vS2,     // Equation Elements
          0
       },
       {  1064.43f, 1665.0f,// Temperature range
          LENGTH(t2vS3), t2vS3,     // Equation Elements
          0
       },
       {  1665.0f, 1780.0f,// Temperature range
          LENGTH(t2vS4), t2vS4,     // Equation Elements
          0
       }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcS[] =
       {
          5.403949350682212E-06,
          1.224788355778571E-08,
           -1.7558428836044E-11
       };
    
    static const double v2tS1[] =
       {
              184.3391846067284,
             -78.72232007414534,
              112.4412261967007,
             -186.6544861615397,
              233.1570804665485,


             -195.0324701550052,
              109.2609145047447,
              -41.5399002001371,
              10.74709361719501,
             -1.863012139764517,
             0.2070624468739163,
           -0.01333615348166221,
          0.0003784658229409919
       };
    
    static const double v2tS2[] =
       {
              138.3054713926352,
             -6.311045318253862,
             0.3990268678944258,
           -0.01151129817047082
       };
    
    static const double v2tS3[] =
       {
               129.458396956766,
             -3.412372738547934,
            0.08238329821851748
       };
    
    static const double v2tS4[] =
       {
             -331.2901982684443,
              75.04210204232305,
             -4.370525320410525,
            0.08424373322146608
       };
    
    static const V2TT v2tS[] =
       {
       {  -0.000235688f,    0.0055521f,   // Voltage Range
          LENGTH(v2tS1), v2tS1      // Equation Elements
       },
       {     0.0055521f,    0.0103343f,   // Voltage Range
          LENGTH(v2tS2), v2tS2      // Equation Elements
       },
       {     0.0103343f,    0.0175358f,   // Voltage Range
          LENGTH(v2tS3), v2tS3      // Equation Elements
       },
       {     0.0175358f,    0.0188248f,   // Voltage Range
          LENGTH(v2tS4), v2tS4      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeS =
       {
       "S",
       DBK14GAIN(Dbk14UniTypeS), DBK14GAIN(Dbk14BiTypeS),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vS), t2vS,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcS), cjcS,
       LENGTH(v2tS), v2tS
       };
    
    static const ThermocoupleT dbk19typeS =
       {
       "S",
       DBK19GAIN(Dbk19UniTypeS), DBK19GAIN(Dbk19BiTypeS),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vS), t2vS,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcS), cjcS,
       LENGTH(v2tS), v2tS
       };

   static const ThermocoupleT tbktypeS =
      {
      "S",
      1.0/200.0, 1.0/200.0,
      #ifdef   INCLUDE_NBS_T2V_DATA
      LENGTH(t2vS), t2vS,
      #endif   // INCLUDE_NBS_T2V_DATA
      LENGTH(cjcS), cjcS,
      LENGTH(v2tS), v2tS
      };

    // DBK81/82
    static const ThermocoupleT dbk81typeS =
       {
       "S",
       DBK81GAIN(Dbk81TypeS), DBK81GAIN(Dbk81TypeS), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vS), t2vS,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcS), cjcS,
       LENGTH(v2tS), v2tS
       };
   
   // DBK90
    static const ThermocoupleT dbk90typeS =
       {
       "S",
       DBK90GAIN(Dbk90TypeS), DBK90GAIN(Dbk90TypeS), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vS), t2vS,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcS), cjcS,
       LENGTH(v2tS), v2tS
       };
   

    #ifdef   INCLUDE_NBS_T2V_DATA
    // Type R Coefficients - -50C thru 630.74
    // Ref: NBS Monograph 125
    // 3/74 Page 34
    
    static const double   t2vR1[] =
       {
       0.,                  5.2891395059E-06,        .013911109947E-06,
       -.00002400523843E-06,    3.6201410595E-14,    -4.4645019036E-17,
       3.8497691865E-20,    -1.5372641559E-23
       };
    
    // Type R Coefficients - 630.74 thru 1064.43C
    // Ref: NBS Monograph 125
    // 3/74 Page 34
    
    static const double   t2vR2[] =
       {
       -264.18007025E-06,       8.0468680747E-06,        .0029892293723E-06,
       -2.6876058617E-13
       };
    
    // Type R Coefficients - 1064.43C thru 1665C
    // Ref: NBS Monograph 125
    // 3/74 Page 34
    
    static const double   t2vR3[] =
       {
       1490.1702702E-06,        2.8639867552E-06,        .0080823631189E-06,
       -.0000019338477638E-06
       };
    
    // Type R Coefficients - 1665C thru 1767.6C ***we're going to 1780C!***
    // Ref: NBS Monograph 125
    // 3/74 Page 34
    
    static const double   t2vR4[] =
       {
       95445.55991E-06,         -166.42500359E-06,       .10975743239E-06,
       -.00002228921698E-06
       };
    
    static const T2VT t2vR[] =
       {
       {  -50.0f, 630.74f, // Temperature range
          LENGTH(t2vR1), t2vR1,     // Equation Elements
          0
       },
       {  630.74f, 1064.43f,// Temperature range
          LENGTH(t2vR2), t2vR2,     // Equation Elements
          0
       },
       {  1064.43f, 1665.0f,// Temperature range
          LENGTH(t2vR3), t2vR3,     // Equation Elements
          0
       },
       {  1665.0f, 1780.0f,// Temperature range
          LENGTH(t2vR4), t2vR4,     // Equation Elements
          0
       }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcR[] =
       {
          5.294212360272556E-06,
          1.359152160610251E-08,
          -1.81304171677004E-11
       };
    
    static const double v2tR1[] =
       {
              188.0854153888949,
             -93.03464875452666,
              147.9916404255369,
             -273.9425298859629,
              379.2146021314036,
             -353.2659713458486,
               222.976219917179,
             -97.21111698354436,
              29.59120329936905,
             -6.273383052496637,
              0.907802992911854,
           -0.08548984274283893,
           0.004722191021006718,
         -0.0001160840860830692
       };
    
    static const double v2tR2[] =
       {
              135.0489562494578,
             -6.846998184560338,
             0.3977282249463465,
           -0.01017988460793719
       };
    
    static const double v2tR3[] =
       {
              125.0904822531307,
             -3.949531694548082,
             0.1159765278592622,
          -0.001036048326825549
       };
    
    static const double v2tR4[] =
       {
              149.1541880741323,
             -5.945866917727339,
             0.1349168067944846
       };
    
    static const V2TT v2tR[] =
       {
       {  -0.000226438f,   0.00593308f,   // Voltage Range
          LENGTH(v2tR1), v2tR1      // Equation Elements
       },
       {    0.00593308f,    0.0113639f,   // Voltage Range
          LENGTH(v2tR2), v2tR2      // Equation Elements
       },
       {     0.0113639f,    0.0197387f,   // Voltage Range

          LENGTH(v2tR3), v2tR3      // Equation Elements
       },
       {     0.0197387f,    0.0212588f,   // Voltage Range
          LENGTH(v2tR4), v2tR4      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeR =
       {
       "R",
       DBK14GAIN(Dbk14UniTypeR), DBK14GAIN(Dbk14BiTypeR),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vR), t2vR,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcR), cjcR,
       LENGTH(v2tR), v2tR
       };
    
    static const ThermocoupleT dbk19typeR =
       {
       "R",
       DBK19GAIN(Dbk19UniTypeR), DBK19GAIN(Dbk19BiTypeR),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vR), t2vR,

       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcR), cjcR,
       LENGTH(v2tR), v2tR
       };

   static const ThermocoupleT tbktypeR =
      {
      "R",
      1.0/200.0, 1.0/200.0,
      #ifdef   INCLUDE_NBS_T2V_DATA
      LENGTH(t2vR), t2vR,
      #endif   // INCLUDE_NBS_T2V_DATA
      LENGTH(cjcR), cjcR,
      LENGTH(v2tR), v2tR
      };

    // DBK81/82
    static const ThermocoupleT dbk81typeR =
       {
       "R",
       DBK81GAIN(Dbk81TypeR), DBK81GAIN(Dbk81TypeR), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vR), t2vR,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcR), cjcR,
       LENGTH(v2tR), v2tR
       };

   // DBK90
    static const ThermocoupleT dbk90typeR =
       {
       "R",
       DBK90GAIN(Dbk90TypeR), DBK90GAIN(Dbk90TypeR), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vR), t2vR,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcR), cjcR,
       LENGTH(v2tR), v2tR
       };

    
    #ifdef   INCLUDE_NBS_T2V_DATA
    // Type B Coefficients - 0C thru 1820C *** using 50C to 1820C ***
    // Ref: NBS Monograph 125
    // 3/74 Page 51
    
    static const double   t2vB1[] =
       {

       0,                   -.2467460162E-06,           .0059102111169E-06,
       -.000001430712343E-06,   2.150914975E-15,        -3.175780072E-18,
       2.4010367459E-21,    -9.0928148159E-25,       1.3299505137E-28
       };
    
    static const T2VT t2vB[] =
       {
       {  50.0f, 1820.0f, // Temperature range
          LENGTH(t2vB1), t2vB1, 0    // Equation Elements
       }
       };
    #endif   // INCLUDE_NBS_T2V_DATA
    
    static const double cjcB[] =
       {
         -2.418094852625476E-07,
          5.740547808814998E-09
       };
    
    static const double v2tB1[] =
       {
              63429.51942221542,
             -34581205.29199016,
              10630028481.19092,
             -1933392773735.719,
              213533502180085.7,
          -1.40537558989013E+16,
          5.064604812844898E+17,
         -7.686672382542055E+18
       };
    
    static const double v2tB2[] =
       {
              13585.56708439602,
             -1151332.370522122,
              62357702.55099584,
             -2150342915.269435,
              49426778949.36398,
             -779233852214.1886,
              8535068911262.018,
             -64811141999513.15,
              334419656733341.9,
              -1118055538981070.,
               2183307235652211.,
              -1889907213698367.
       };
    
    static const double v2tB3[] =
       {
              2629.265595375172,
             -17601.00898156647,
              85976.85123643593,
             -281948.5393821176,
              644328.5907990847,
             -1059470.390581207,
              1282169.846450827,
               -1158850.8275231,
              788201.5396345838,
             -403754.2179629622,
              154713.9812360035,
             -43625.98418402705,
              8778.340042503087,
             -1192.038079624323,
               97.8431786782966,
             -3.665211406513323
       };
    
    static const double v2tB4[] =
       {
              579.4124503349055,
             -227.0414131018019,
              67.97805401839408,
             -13.28971433779539,
              1.707339401586383,
            -0.1430103829876957,
           0.007517857860144534,
          -0.000225079488833917,
          2.926793785055317E-06
       };
    
    static const V2TT v2tB[] =
       {
       {   2.27188E-06f,  1.41963E-05f,   // Voltage Range
          LENGTH(v2tB1), v2tB1      // Equation Elements
       },
       {   1.41963E-05f,  0.000178181f,   // Voltage Range
          LENGTH(v2tB2), v2tB2      // Equation Elements
       },
       {   0.000178181f,   0.00315403f,   // Voltage Range
          LENGTH(v2tB3), v2tB3      // Equation Elements
       },
       {    0.00315403f,    0.0133545f,   // Voltage Range
          LENGTH(v2tB4), v2tB4      // Equation Elements
       }
       };
    
    static const ThermocoupleT dbk14typeB =
       {
       "B",
       DBK14GAIN(Dbk14UniTypeB), DBK14GAIN(Dbk14BiTypeB),
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vB), t2vB ,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcB), cjcB,
       LENGTH(v2tB), v2tB };
    
    static const ThermocoupleT dbk19typeB =
       {
       "B",
       DBK19GAIN(Dbk19UniTypeB), DBK19GAIN(Dbk19BiTypeB),

       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vB), t2vB ,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcB), cjcB,
       LENGTH(v2tB), v2tB };

   static const ThermocoupleT tbktypeB =
      {
      "B",
      1.0/200.0, 1.0/200.0,
      #ifdef   INCLUDE_NBS_T2V_DATA
      LENGTH(t2vB), t2vB ,
      #endif   // INCLUDE_NBS_T2V_DATA
      LENGTH(cjcB), cjcB,
      LENGTH(v2tB), v2tB
      };

    // DBK81/82
    static const ThermocoupleT dbk81typeB =
       {
       "B",
       DBK81GAIN(Dbk81TypeB), DBK81GAIN(Dbk81TypeB), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vB), t2vB,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcB), cjcB,
       LENGTH(v2tB), v2tB
       };


    // DBK81/82
    // CJC linearization : done different. Included here for completeness but NULL
    static const ThermocoupleT dbk81typeCJC =
       {
       "CJC",
       1.0, 1.0, // Scaling HERE!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       0, NULL,
       #endif   // INCLUDE_NBS_T2V_DATA
       0, NULL,
       0, NULL
       };
    
   // DBK90
    static const ThermocoupleT dbk90typeB =
       {
       "B",
       DBK90GAIN(Dbk90TypeB), DBK90GAIN(Dbk90TypeB), // No Unipolar!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       LENGTH(t2vB), t2vB,
       #endif   // INCLUDE_NBS_T2V_DATA
       LENGTH(cjcB), cjcB,
       LENGTH(v2tB), v2tB
       };


    // DBK90
    // CJC linearization : done different. Included here for completeness but NULL
    static const ThermocoupleT dbk90typeCJC =
       {
       "CJC",
       1.0, 1.0, // Scaling HERE!!!
       #ifdef   INCLUDE_NBS_T2V_DATA
       0, NULL,
       #endif   // INCLUDE_NBS_T2V_DATA
       0, NULL,
       0, NULL
       };

      /* for dbk81/82 thermistor2Temp() */

      static const double   A = 1.12735468168042E-3;
      static const double   B = 2.34397822685416E-4;
      static const double   C = 8.67484773788255E-8;


    /* Structure pointer array definition updated for TempBook/66 support */
    
       static const ThermocoupleT  *thermocouple[] =
          {
          &dbk14typeJ,     // Order must match #define values!
          &dbk14typeK,
          &dbk14typeT,
          &dbk14typeE,
          &dbk14typeN28,
          &dbk14typeN14,
          &dbk14typeS,
          &dbk14typeR,
          &dbk14typeB,
          &dbk19typeJ,
          &dbk19typeK,
          &dbk19typeT,
          &dbk19typeE,
          &dbk19typeN28,
          &dbk19typeN14,
          &dbk19typeS,
          &dbk19typeR,
          &dbk19typeB,
          &tbktypeJ,        
          &tbktypeK,      
          &tbktypeT,
          &tbktypeE,
          &tbktypeN28,
          &tbktypeN14,
          &tbktypeS,
          &tbktypeR,
          &tbktypeB,
          &dbk81typeJ,
          &dbk81typeK,
          &dbk81typeT,
          &dbk81typeE,
          &dbk81typeN28,
          &dbk81typeN14,
          &dbk81typeS,
          &dbk81typeR,
          &dbk81typeB,
          &dbk81typeCJC,  // CJC linearization : done different. Included here for completeness but NULL
		  &dbk90typeJ,
          &dbk90typeK,
          &dbk90typeT,
          &dbk90typeE,
          &dbk90typeN28,
          &dbk90typeN14,
          &dbk90typeS,
          &dbk90typeR,
          &dbk90typeB,
          &dbk90typeCJC,
          };
    
    
    #define  nthermocouple  LENGTH(thermocouple)
    
    static TCSetupT tcSetup = { 0 };
    static RtdSetupT rtdSetup = { 0 };
//    static LinearSetupT linearSetup = { 0 };
    int
    Poly(double x, int degree, CoeffT *coeff)
    {
       int   i;
    
       result = coeff[degree].value;
    
       for (i=degree-1;i>=0;--i) {
          result = result * x + coeff[i].value;
       }
    
       return 0;
    }
    
    DAQAPI DaqError WINAPI
    daqCvtTCSetup(DWORD nscan, DWORD cjcPosition,DWORD ntc,
       TCType tcType, BOOL bipolar, DWORD avg)
    {   
       // wjj: rearrange for dbk81/82 cjc only conversion



       //first get tctype
       if (tcType >= nthermocouple)
          return apiCtrlProcessError(-1, DerrTCE_TYPE);

       tcSetup.tcType = thermocouple[tcType];

       // now check limits (TC or CJC usage)
       if (tcSetup.tcType == thermocouple[Dbk81CJCType]) {
       // CJC
          if (nscan == 0                   ||
              nscan > 512                  ||
              cjcPosition > nscan - 1      ||
              /* ntc == 0                  ||*/
              ntc > nscan - 1
             ) { return apiCtrlProcessError(-1, DerrTCE_PARAM); }

       } else {
       // TC
          if (nscan == 0                   ||
              nscan > 512                  ||
              cjcPosition > nscan - 2      ||
              ntc == 0                     ||
              ntc > nscan - 1 - cjcPosition
             ) { return apiCtrlProcessError(-1, DerrTCE_PARAM); }
       }

       // Get the hardware type used for the current TC setup from the tcType.
       if (tcType < Dbk19TCTypeJ)          // TCtypes 0-8 (Dbk14)
          tcSetup.hwType = DtshtDbk14;
       else if (tcType < TbkTCTypeJ)       // TCtypes 9-17 (Dbk19, Dbk52) 
          tcSetup.hwType = DtshtDbk19;
       else if (tcType < Dbk81TCTypeJ)     // TCtypes 18-26 (TempBook/66)
          tcSetup.hwType = DtshtTbk66;            
       else if (tcType < Dbk90TCTypeJ)  
          tcSetup.hwType = DtshtDbk81;     // TCtypes 27-36 (DBK81/82)
	   else
		  tcSetup.hwType = DtshtDbk90;
		

       // and the rest...
       tcSetup.nscan = nscan;
       tcSetup.cjcPosition = cjcPosition;
       tcSetup.ntc = ntc;                  
       tcSetup.bipolar = bipolar;
       tcSetup.avg = avg;

/*
       if (nscan == 0                   ||
           nscan > 512                  ||
           cjcPosition > nscan - 2      ||
           ntc == 0                     ||
           ntc > nscan - 1 - cjcPosition
          ) { return apiCtrlProcessError(-1, DerrTCE_PARAM); }
       if (tcType >= nthermocouple)
          return apiCtrlProcessError(-1, DerrTCE_TYPE);
    
       tcSetup.nscan = nscan;
       tcSetup.cjcPosition = cjcPosition;
       tcSetup.ntc = ntc;
       tcSetup.tcType = thermocouple[tcType];

       // Get the hardware type used for the current TC setup from the tcType.
       if (tcType < Dbk19TCTypeJ)          // TCtypes 0-8 (Dbk14)
          tcSetup.hwType = DtshtDbk14;
       else if (tcType < TbkTCTypeJ)       // TCtypes 9-17 (Dbk19, Dbk52) 
          tcSetup.hwType = DtshtDbk19;
       // else                             
       else if (tcType < Dbk81TCTypeJ)     // TCtypes 18-26 (TempBook/66)
          tcSetup.hwType = DtshtTbk66;            
       else
          tcSetup.hwType = DtshtDbk81;     // TCtypes 27-36 (DBK81/82)
                  
       tcSetup.bipolar = bipolar;
       tcSetup.avg = avg;
*/    
       return DerrNoError;
    }
    
    #ifdef   _Pascal
    
    const TCSetupT * WINAPI
    fetch_TCSetup(void)
    {
       return &tcSetup;
    }
    
    #elif defined(_QBasic)
    
    const TCSetupT * WINAPI
    daqTCfetchSetup(void)
    {
       return &tcSetup;
    }
    
    void * _Cdecl __memcpy__(void *, const void *, unsigned);
    
    #pragma option -Oi


///////////////////////////////////////////////////////////////////////////////////////////////
//daqCvtSetupConver
///////////////////////////////////////////////////////////////////////////////////////////////    
void WINAPI daqIndex(void *r, void *origin, unsigned size, unsigned index)
    {
       __memcpy__(r, (char *)origin+size*index, size);
    }
    
    #else
    
    // CJC temperature to voltage with error checking
    static int
    temp2voltage(VoltageT *voltage, double temp, const ThermocoupleT *tcType)
    {
       voltage->value = 0.0f;
    
	   // MODIFIED TO ALLOW CJC TEMPS BELOW 0 DEG C and over 100
       if (temp < -40.0f || temp > 100.0f)
       { 
          return apiCtrlProcessError(-1, DerrTCE_TRANGE); 
       }
       // TODO VERIFY for dbk81
	   // if (temp < -50.0f || temp > 150.0f) { return apiCtrlProcessError(-1, DerrTCE_TRANGE); }
	   // if (temp < 0.0f || temp > 100.0f) { return apiCtrlProcessError(-1, DerrTCE_TRANGE); }
      //coeff = (double *)tcType->cjc;
    
       Poly(temp, tcType->ncjc - 1,(CoeffT *)tcType->cjc);
    
       voltage->value = temp * result;
    
       return 0;
    }
    
    //#ifdef   INCLUDE_NBS_T2V_DATA
	#ifdef INCLUDE_T2V_DATA_FUNCTIONS
    
    static double
    segVoltage(double temp, const T2VT *seg)
       {
    
       //coeff = seg->coeff;
    
       Poly(temp, seg->ncoeff - 1,(CoeffT *)seg->coeff );
       if (seg->expTerm)
          {
          double x = (temp - 127.)/65.;
          result +=  125E-6 * exp(-.5*x*x);
          }
       return result;
       }
    
    static double
    tcVoltage(double temp, const ThermocoupleT *tcType)
       {
       // Need error return if out of range
       int   i;
       for (i=0; i < tcType->nt2v; ++i)
          {
          if (temp <= tcType->t2v[i].endTemp) { break; }
          }
       if (! (i < tcType->nt2v)) { --i; }
       return segVoltage(temp, &tcType->t2v[i]);
       }
    
    #endif
    
    #ifdef   INCLUDE_TEMP_RANGE
    
    static double
    segTemp(double volt, const V2TT *seg)
    {
       volt = 1000.f * volt;
    
       //coeff = (double *)seg->coeff;
    
       Poly(volt, seg->ncoeff - 1,(CoeffT *)seg->coeff );
       return  volt * result;
    }
    
    static double
    {
       // Need error return if out of range
       int   i;
       for (i=0; i < tcType->nv2t; ++i)
          {
          if (volt <= tcType->v2t[i].endVolt) { break; }
          }
       if (! (i < tcType->nv2t)) { --i; }
       return segTemp(volt, &tcType->v2t[i]);
    }

    
    #endif   // INCLUDE_TEMP_RANGE
    
    static int
    voltage2temp(TempT *temp, double voltage, const ThermocoupleT *tcType)
    {
       const V2TT  *seg;
       int      i;
       double    v;
    
       if (voltage < tcType->v2t[0].startVolt)
       { 
   #ifdef OEC
          //wjj:11.27.1:apiCtrlProcessError deferred!!!          
          return DerrTCE_VRANGE;
   #else
          return apiCtrlProcessError(-1, DerrTCE_VRANGE);
   #endif
       }
    
       for (i=0; i < tcType->nv2t; ++i)
          {
          if (voltage <= tcType->v2t[i].endVolt) { break; }
          }
       if (! (i < tcType->nv2t))
       {
   #ifdef OEC
          //wjj:11.27.1:apiCtrlProcessError deferred!!!          
          return DerrTCE_VRANGE;
   #else
          return apiCtrlProcessError(-1, DerrTCE_VRANGE);
   #endif          
       }
    
       seg = &tcType->v2t[i];
       v = 1000.f * voltage;

       //coeff = (double *)(seg->coeff);
    
       Poly(v, seg->ncoeff - 1,(CoeffT *)seg->coeff);
       temp->value =  v * result;
    
       return 0;
    }

    /****************************   
    thermistor2Temp: DBK81 CJC V2T conversion
    Function to calculate Temp from DBK81/82 thermistor - mod as needed
    *****************************/
    // static int // cant be static
    int
    thermistor2Temp(double wRawCount, BOOL biPolar, double *temp)
    {       

      double   Vo  = 0.0f; 
      double   Vth = 0.0f;
      double   Rth = 0.0f;
      double   Tdc = 0.0f;

      /* for dbk81/82 thermistor2Temp() */
      // defined above
      // static const double   A = 1.12735468168042E-3;
      // static const double   B = 2.34397822685416E-4;
      // static const double   C = 8.67484773788255E-8;
   
      Vo = (wRawCount - 32768.0) * (5.0/32767.0) + ( biPolar?0.0:5.0 );
      Vth = Vo * 1.0004;   
      // NOTICE : Vth MUST BE LESS THAN (<) 4.096 !!!
      // AND GREATER THAN (>) 0 !!!
      if ( (Vth <= 0) || (Vth >= 4.096) ) {
         return (DerrTCE_TRANGE);
      }

      Rth = (Vth * 23200) / (4.096 - Vth);   
      Tdc = ( 1.0 / ( A + (B * log(Rth)) + (C * pow(log(Rth),3)) ) ) - 273.15;

      *temp = Tdc;

      return 0;
    }
    
    // Convert counts to clean value from 0 to 65535 : or not
    //#define  MASK(x)     (x & ~0x0f)
    #define  MASK(x)     (x)
    
    /************************************************************
    * User API TC Function Implementations
    ************************************************************/
    
///////////////////////////////////////////////////////////////////////////////////////////////
//daqCvtTCConvert
///////////////////////////////////////////////////////////////////////////////////////////////    
DAQAPI DaqError WINAPI daqCvtTCConvert(PWORD counts, DWORD scans, PSHORT temp,
       DWORD ntemp)
    {
       double cjcGain;
       double tcGain;
    
       double offset;
    
       double cjcOffset;
       double tcOffset;
    
       double sum;    // Changed to double to prevent overflow when many (>32k samples are averaged).
    
       VoltageT cjcVoltage;
       double cjcTemp;
       double tcVoltage;
       TempT tcTemp;
    
       DaqError   err;
       unsigned i, j, k;
    
       double cjcV2T;

       
       //wjj:11.27.1:apiCtrlProcessError deferred!!!
       DaqError   runningDaqError = DerrNoError;
    
       // Select between CJC voltage to Temperature conversions
       // depending on the hardware type (dbk14, dbk19/52, TempBook/66)
       switch (tcSetup.hwType) {
          case DtshtDbk14:    // Dbk14
             cjcV2T = 1.0/0.0244;
             break;
          case DtshtDbk19:    // Dbk19/Dbk52
             cjcV2T = 600.0;
             break;
          case DtshtTbk66:    // TempBook/66
             cjcV2T = 500.0;
             break;
          case DtshtDbk90:
          case DtshtDbk81:
          default:            // DBK81/82
             cjcV2T = 1.0;             
             break;
        }
  
       if (!tcSetup.nscan) { return apiCtrlProcessError(-1, DerrTCE_NOSETUP); }
    
//       Max scans 32768 does not apply in Win32. Do not check number of scans.
      if (ntemp < tcSetup.ntc          ||
        (tcSetup.avg && ntemp < scans * tcSetup.ntc)
       ) { return apiCtrlProcessError(-1, DerrTCE_PARAM); }
           
      // only for 19s & TBKs WITH autoZero
      if ( ((tcSetup.hwType == DtshtTbk66) || (tcSetup.hwType == DtshtDbk19)) && (session->zeroDbk19)) {
          
          if (tcSetup.cjcPosition<2) { return apiCtrlProcessError(-1, DerrTCE_PARAM); }
          
          // Perform zero-compensation on the CJC reading
          err = daqZeroSetupConvert(tcSetup.nscan, tcSetup.cjcPosition-2, tcSetup.cjcPosition, 1, counts, scans); // SMS
          
          if (err) { return err; }

          // Perform zero-compensation on the TC reading(s)
          err = daqZeroSetupConvert(tcSetup.nscan, tcSetup.cjcPosition-1, tcSetup.cjcPosition+1, tcSetup.ntc, counts, scans); // SMS
          
          if (err) { return err; }
       }
    
       // Shift counts to place the cjc at the first element
       counts += tcSetup.cjcPosition;
    
       // Compute the offset and inverse gains.
       if(tcSetup.bipolar)
       {
          //#ifdef CDQ_BLD  //Cardaq has -10 to +10 volt range
          //   offset = -10.0;
          //#else
          //   offset = -5.0;
          //#endif

          switch (tcSetup.hwType) {
             case DtshtDbk19:       // DaqBook/Board cards 
             case DtshtDbk14:       // DaqBook/Board cards 
             case DtshtTbk66:       // TempBook/66
             case DtshtDbk81:       // Dbk81/82
			 case DtshtDbk90:
                offset = -5.0;
                break;
             default:
                offset = -5.0;
                break;
             }

          switch (tcSetup.hwType) {             
             case DtshtDbk14:  // Dbk14
                cjcGain = DBK14GAIN(Dbk14BiCJC);
                break;
             case DtshtDbk19:  // Dbk19/Dbk52
                cjcGain = DBK19GAIN(Dbk19BiCJC);
                break;
             // default:          // TempBook/66
             //   cjcGain = 1.0/50.0;

             case DtshtTbk66:    // TempBook/66
                cjcGain = 1.0/50.0;
                break;
			 case DtshtDbk90:
             case DtshtDbk81:
             default:            // DBK81/82
                cjcGain = 1.0;
                break;
          }
          tcGain = tcSetup.tcType->biGain;
       }
       else
       {
          offset = 0.0;
          switch (tcSetup.hwType) {
             case DtshtDbk14:  // Dbk14
                cjcGain = DBK14GAIN(Dbk14UniCJC);
                break;
             case DtshtDbk19:  // Dbk19/Dbk52
                cjcGain = DBK19GAIN(Dbk19UniCJC);
                break;
             // default:          // TempBook/66
             //   cjcGain = 1.0/100.0;

             case DtshtTbk66:    // TempBook/66
                cjcGain = 1.0/100.0;
                break;
			 case DtshtDbk90:
             case DtshtDbk81:
             default:            // DBK81/82
                cjcGain = 1.0;
                break;

          }
          tcGain = tcSetup.tcType->uniGain;
       }
    
       // Now cjcVoltage = cjcGain*(sum/6553.6/navg+offset)
       // and cjcTemp = cjcVoltage/0.0244
       // so cjcTemp = sum * cjcGain / 6553.6 / 0.0244 / navg +
       //              cjcGain * offset / 0.0244
       // NOTE: Cardaq's ADC has a different LSB resolution then DaqBook/Board
       //       Cardaq 20V range
       //       DaqBook/Board 10V range
    
       cjcOffset = cjcGain * offset * cjcV2T;
    
       //#ifdef CDQ_BLD
       //   cjcGain = cjcGain / 3276.8 * cjcV2T;
       //#else
       //   cjcGain = cjcGain / 6553.6 * cjcV2T;
       //#endif
    
       switch (tcSetup.hwType) {
          case DtshtDbk19:       // DaqBook/Board cards 
          case DtshtDbk14:       // DaqBook/Board cards 
          case DtshtTbk66:       // TempBook/66
             cjcGain = cjcGain / 6553.6 * cjcV2T;
             break;
          case DtshtDbk90:			//Dbk90
          case DtshtDbk81:         // Dbk81/82
             cjcGain = 1;
             break;
          
          default:
             cjcGain = cjcGain / 6553.6 * cjcV2T;
             break;
       }

       // Similar for tc voltages
       tcOffset = tcGain * offset;

       //#ifdef CDQ_BLD
       //   tcGain = tcGain / 3276.8;
       //#else
       //   tcGain = tcGain / 6553.6;
       //#endif
    
       switch (tcSetup.hwType) {
          // TODO : ADD DBK81/82
          case DtshtDbk19:       // DaqBook/Board cards 
          case DtshtDbk14:       // DaqBook/Board cards 
          case DtshtTbk66:       // TempBook/66
          case DtshtDbk81:       // Dbk81
		  case DtshtDbk90:
             tcGain = tcGain / 6553.6;
             break;
          default:
             tcGain = tcGain / 6553.6;
             break;
       }

       //fprintf(test,"TC GAIN |%f| TC OFFSET |%f| CJC GAIN |%f| CJC OFFSET |%f|\n",
       //tcGain,tcOffset,cjcGain,cjcOffset);
    
       // Convert the data here.
       switch (tcSetup.avg)
          {
       case  0:
          // Block averaging
          // First compute the physical cjc temperature
          sum = 0.0;
          for (i=0; i<scans; ++i)
             { sum += (double)MASK(counts[i*tcSetup.nscan]); }
          
          if (tcSetup.hwType == DtshtDbk81 || tcSetup.hwType == DtshtDbk90)
          {
             // Special DBK81/82 thermistor linearization - sorry.
             err = (DaqError) thermistor2Temp( (sum / scans), tcSetup.bipolar, &cjcTemp); //Dbk81CJCType
             if (err) { return err; }
             
			 // Get temp2voltage if NOT CJC
			 //ecg - added dbk90 cjc check
             if (
						(tcSetup.tcType != thermocouple[Dbk81CJCType])
				  &&	(tcSetup.tcType != thermocouple[Dbk90CJCType])
				)
               err = (DaqError)temp2voltage(&cjcVoltage, cjcTemp, tcSetup.tcType);
          }
          else {          
             // original
             cjcTemp = cjcGain * sum / scans + cjcOffset;

             // Now set the cjcTemp to the tc equivalent voltage
             // at the cjc temperature.
    
             err = (DaqError)temp2voltage(&cjcVoltage, cjcTemp, tcSetup.tcType);
          }
        
          if (err) { return err; }
          // Now, for each tc channel
          for (j = 1; j <= tcSetup.ntc; ++j)
             {
                // wjj:7.23.01:quick dbk81/82 cjc temp request...
                if ((tcSetup.hwType == DtshtDbk81) && (tcSetup.tcType == thermocouple[Dbk81CJCType]))
                {
                   //return CJC temp                                 
                   temp[j - 1] = (int)((cjcTemp * 10.0) + 0.5); // x10 required >>> WITH ROUNDING!!!!!
                }

				//corresponding dbk90 check also
				else if ((tcSetup.hwType == DtshtDbk90) && (tcSetup.tcType == thermocouple[Dbk90CJCType]))
                {
                   //return CJC temp                                 
                   temp[j - 1] = (int)((cjcTemp * 10.0) + 0.5); // x10 required >>> WITH ROUNDING!!!!!
                }
                else
                {
                   // First compute the physical tc voltage
                   sum = 0.0;
                   for (i=0; i<scans; ++i)
                      { sum += (double)MASK(counts[i*tcSetup.nscan + j]); }
                   tcVoltage = tcGain * sum / scans + tcOffset;
      
                   // Now compute the cjc corrected temperature
                   err = (DaqError)voltage2temp(&tcTemp, tcVoltage+cjcVoltage.value, tcSetup.tcType);

            //wjj:11.27.1:apiCtrlProcessError deferred!!!
            #ifdef OEC
                   if (err) {
                      temp[j - 1] = (WORD) TC_OVERRANGE_VAL;
                      runningDaqError = err;
                      continue;
                      // return apiCtrlProcessError (-1, err);
                   }
            #else
                   if (err) { return err; }
            #endif    
                   // Finally, round and store the result
                   temp[j - 1] = (int)(10.0*tcTemp.value + 0.5);

                }
             }
          break;
    
       case 1:
          // No averaging
          // For each scan
          for (i=0; i<scans; ++i)
             {
             // First compute the physical cjc voltage
             if (tcSetup.hwType == DtshtDbk81 || tcSetup.hwType == DtshtDbk90)
             {
                // Special DBK81/82 thermistor linearization - sorry.
                err = (DaqError) thermistor2Temp( MASK(counts[i*tcSetup.nscan]), tcSetup.bipolar, &cjcTemp);
                if (err) { return err; }
                // Get temp2voltage if NOT CJC
                if (tcSetup.tcType != thermocouple[Dbk81CJCType] && tcSetup.tcType != thermocouple[Dbk90CJCType])
                  err = (DaqError)temp2voltage(&cjcVoltage, cjcTemp, tcSetup.tcType);
             }
             else {          
                // origional
                cjcTemp = cjcGain * MASK(counts[i*tcSetup.nscan]) + cjcOffset;

                // Now set the cjcTemp to the tc equivalent voltage
                // at the cjc temperature.
             
                err = (DaqError)temp2voltage(&cjcVoltage, cjcTemp, tcSetup.tcType);
             }    
             if (err) { return err; }
       
             // Now, for each tc channel
             for (j = 1; j <= tcSetup.ntc; ++j)
                {
                   // wjj:7.23.01:quick dbk81/82 cjc temp request...
					//ecg: 8.13.03 - added dbk90 cjc to the test since it should be using the same routine
						//Otherwise DaqX crashes since the tcSetup.tcType->cjc is NULL like the 81/82
                   if(
							((tcSetup.hwType == DtshtDbk81) && (tcSetup.tcType == thermocouple[Dbk81CJCType]))
					   ||	((tcSetup.hwType == DtshtDbk90) && (tcSetup.tcType == thermocouple[Dbk90CJCType]))
					 )
                   {
                      //return CJC temp
                      temp[i*tcSetup.ntc + j - 1] = (int)((cjcTemp * 10.0) + 0.5); // x10 required >>> WITH ROUNDING!!!!!
                   }



                   else
                   {
                      // First compute the physical tc voltage
                      tcVoltage = tcGain* MASK(counts[i*tcSetup.nscan + j]) + tcOffset;
                   
                      // Now compute the cjc corrected temperature
                      err = (DaqError)voltage2temp(&tcTemp, tcVoltage+cjcVoltage.value, tcSetup.tcType);

               //wjj:11.27.1:apiCtrlProcessError deferred!!!
               #ifdef OEC
                      if (err) {
                         temp[i*tcSetup.ntc + j - 1] = (WORD) TC_OVERRANGE_VAL;
                         runningDaqError = err;
                         continue;
                         // return apiCtrlProcessError (-1, err);
                      }
               #else
                      if (err) { return err; }
               #endif    
    
                      // Finally, round and store the result
                      temp[i*tcSetup.ntc + j - 1] = (int)(10.0*tcTemp.value + 0.5);
                   }    
                }
             }
          break;
    
       default:
          // Moving average
          // For each scan from the last to the first
          for (i= scans; i-- > 0; )
             {
             // Compute the actual number to average
             unsigned  avg;
             if (tcSetup.avg > i) { avg = i+1; } else { avg = tcSetup.avg; }
    
             // Average the last avg scans
             // First the cjc
             sum = 0.0;
             for (k=0; k < avg; ++k)
                { sum += (double)MASK(counts[(i-k)*tcSetup.nscan]); }

             if (tcSetup.hwType == DtshtDbk81)
             {
                // Special DBK81/82 thermistor linearization - sorry.
                err = (DaqError) thermistor2Temp( (sum / avg), tcSetup.bipolar, &cjcTemp); //Dbk81CJCType
                if (err) { return err; }
                // Get temp2voltage if NOT CJC

                if (tcSetup.tcType != thermocouple[Dbk81CJCType] || tcSetup.tcType != thermocouple[Dbk90CJCType])
                  err = (DaqError)temp2voltage(&cjcVoltage, cjcTemp, tcSetup.tcType);
             }
             else {          
                // origional
                cjcTemp = cjcGain * sum / avg + cjcOffset;

                // Now set the cjcVoltage to the tc equivalent voltage
                // at the cjc temperature.
                err = (DaqError)temp2voltage(&cjcVoltage, cjcTemp, tcSetup.tcType);
             }    
             if (err) { return err; }
    
             // Now, for each tc channel
             for (j = 1; j <= tcSetup.ntc; ++j)
                {
                   // wjj:7.23.01:quick dbk81/82 cjc temp request...
                   if ((tcSetup.hwType == DtshtDbk81) && (tcSetup.tcType == thermocouple[Dbk81CJCType]))
                   {
                      //return CJC temp
                      temp[i*tcSetup.ntc + j - 1] = (int)((cjcTemp * 10.0) + 0.5); // x10 required >>> WITH ROUNDING!!!!!
                   }
                   else if ((tcSetup.hwType == DtshtDbk90) && (tcSetup.tcType == thermocouple[Dbk90CJCType]))
                   {
                      //return CJC temp
                      temp[i*tcSetup.ntc + j - 1] = (int)((cjcTemp * 10.0) + 0.5); // x10 required >>> WITH ROUNDING!!!!!
                   }
					else
                   {
                      // First compute the physical tc voltage
                      sum = 0.0;
                      for (k=0; k < avg; ++k)
                         { sum += (double)MASK(counts[(i-k)*tcSetup.nscan + j]); }
                      tcVoltage = tcGain * sum / avg + tcOffset;
    
                      // Now compute the cjc corrected temperature
                      err = (DaqError)voltage2temp(&tcTemp, tcVoltage+cjcVoltage.value, tcSetup.tcType);

               //wjj:11.27.1:apiCtrlProcessError deferred!!!
               #ifdef OEC
                      if (err) {
                         temp[i*tcSetup.ntc + j - 1] = (WORD) TC_OVERRANGE_VAL;
                         runningDaqError = err;
                         continue;
                         // return apiCtrlProcessError (-1, err);
                      }
               #else
                      if (err) { return err; }
               #endif    
    
                      // Finally, round and store the result
                      temp[i*tcSetup.ntc + j - 1] = (int)(10.0*tcTemp.value + 0.5);
                   }
                }
             }
          }

   #ifdef OEC
       if (runningDaqError)
       {
          return apiCtrlProcessError (-1, runningDaqError);
       }
   #endif
    
       return DerrNoError;
    }
    
    #ifndef _QBasic

///////////////////////////////////////////////////////////////////////////////////////////////
//daqCvtSetupConver
///////////////////////////////////////////////////////////////////////////////////////////////    
DAQAPI DaqError WINAPI  daqCvtTCSetupConvert(DWORD nscan, DWORD cjcPosition, DWORD ntc,
       TCType tcType, BOOL bipolar, DWORD avg, PWORD counts,
       DWORD scans, PSHORT temp, DWORD ntemp)
    {
       DaqError   err = daqCvtTCSetup(nscan, cjcPosition, ntc, tcType,
                              bipolar, avg);
       if (err) { return err; }
    
       return daqCvtTCConvert(counts, scans, temp, ntemp);
    }
    
    #endif   // _QBasic
    
    #ifdef   INCLUDE_TEMP_RANGE
    double
    startTemp(const ThermocoupleT *tcType)
       {
       return tcTemp(tcType->v2t[0].startVolt, tcType);
       }
    
    double
    endTemp(const ThermocoupleT *tcType)
       {
       return tcTemp(tcType->v2t[tcType->nv2t-1].endVolt, tcType);
       }
    #endif   //INCLUDE_TEMP_RANGE
    
    /************************************************************
    * Local RTD Function Implementations
    ************************************************************/
    

    WORD
    rndULongDivUInt(unsigned long theLong, WORD theInt)
    {
       // WORD hLong, lLong; // SMS
    
       // don't allow divide by zero
       if (theInt==0) {return 0;}
       // don't allow overflow e rror
       if (HighWord(theLong)>=theInt)
        { return 0xffff; }

       //wjj:1.25.2:removal of assembly

       return (WORD)((theLong + (theInt / 2)) / theInt);

       /*
       hLong = HighWord(theLong);
       lLong = LowWord(theLong);
       
       _asm {
          mov dx, hLong
          mov ax, lLong
          mov cx, theInt
          // quoient in AX  remainder in DX
          div cx
          // if remainder greater than half dividsor
          // then round quoient up
          shr cx,1
          adc cx,0
          cmp cx,dx
          jg  end
          add ax,1
          // don't let a wrap around on round up (round back down)
          jnz end
          sub ax,1
       }
    end:
    
       _asm {
          mov lLong, ax
       }
    
       return lLong;
       */
    }
    
 
    static WORD rtdResistance2Temp(WORD resistance)
       {
          WORD index, slope, tmpRes, tmpOffset;
          DWORD resXslope;
    
          //the index is the top 6 bits of the resistance value
          // multiplied by 2 because there are 2 values for each
          // interval in the RTD curve array
          index = (resistance >> 10) * RtdCurveValues;
    
          // calculate fraction part of resistance times slope
          // divided by interval width (1024 or 2^10)
          tmpRes = resistance & 0x3ff;
          slope = RtdPt385[index + RtdCurveSlope];
          tmpOffset = RtdPt385[index + RtdCurveOffset];
    
          resXslope = (DWORD)tmpRes * (DWORD)slope;
          tmpRes = (WORD) ( ( (short) ((resXslope >> 10) + tmpOffset) / 4) + 1500);


          //wjj:1.25.2:removal of assembly
          /*
          #if 0
                _asm {
                   mov ax, tmpRes // SMS
                   mov bx, slope
                   imul bx  //resistance * slope in dx, ax
                   //
                   //// long divide by 2^10 for interval size
                   mov cx, 10
                }
          again:
                _asm {
                   shr dx,1 // SMS
                   rcr ax,1
                   loop again
                   //// round result in ax (add carry) and add
                   //// the temperature offset
                   adc ax,0
                   mov bx, tmpOffset
                   add ax,bx
                   ////temp is in ax @ 0.025 resolution divide by 4
                   //// to convert 0.025 (degree per bit ) resolution
                   //// to 0.1 resolution
                   mov cl,2
                   sar ax,cl
                   //// round and add 1500 because temperatures in table
                   //// are offset by 150 degrees (1500 tenths of a degree) to
                   //// accomedate 0.025 resolution (in an integer - temp range -200 to 850)    }
                   adc ax,1500
                   mov tmpRes, ax
                }
          #endif    
          */

          // resolution 0.1 degrees per bit temp offset 0 degrees
          return tmpRes;
    
       }
    
  
    // HJB 8/29/96 keep 16bit values
    static WORD rtdCounts2Resistance(WORD voltA, WORD voltB, WORD voltD)
    #define RSense 100
    #define RMult100 160
    #define RMult500 32
    #define RMult1K 16
    
       {
       // the formula to convert counts to resistance is
       // (va - vd - 2*(va-vb)) / vd  * rSense
       // multipling this by the correct scaling factor for
       // the RTD type coverts it to the form that
       // rtdResistance2Temp wants it in
    
          unsigned long tLong;
          
          //HJB 8/29/96 keep 16bit values
          WORD tempCounts, tempMult, tempType;
    
          // Note:: The Cardaq has a different scaling factor and
          // voltage range then the DaqBook/Board. The scaling factor
          // does not influence the result of this routine because the formula
          // to calculate resistance is a ratio of the counts.  However the
          // different voltage range does incfluence the calculation because the
          // voltages for the DBK9 are normalized to -5V.  This introduces an
          // additional offset because the -5V reference does not corresponde to
          // 0 counts in the Cardaq.  To compensate for this it is necessary to
          // subtract out the 5V offset (16834 counts).  If this module is to
          // be used with both daqbook and cardaq products it will be necessary to
          // conditionally compile the next 3 lines for cardaq only.
          
          // commented out for daqboard, used for cardaq 
          // enum changes in daqx.h obsolete following 3 lines
          //voltA -= 16384;
          //voltB -= 16384;
          //voltD -= 16384;
    
          switch (rtdSetup.rtdValue) {
             case Dbk9RtdType100: tempType = RMult100;
                                  break;
             case Dbk9RtdType500: tempType = RMult500;
                                  break;
             case Dbk9RtdType1K:  tempType = RMult1K;
                                  break;
          }
    

          // assume this can't overflow because
          // for valid readings vA > vD && vA > vB
          tempCounts = voltA - voltD - 2*(voltA-voltB); //numerator
          tempMult = tempType * RSense; // max 32000     //more numerator
    
          //wjj:1.25.2:removal of assembly
          tLong = tempCounts;
          tLong *= tempMult;

          /*
          // this next multiply may overflow into a long
          _asm {
             mov ax, tempCounts // SMS
             mov dx, tempMult
             mul dx
             mov tempCounts, ax
             mov tempMult, dx
          }           
          // store it in a long
          HighWord(tLong) = tempMult;
          LowWord(tLong) = tempCounts;
          */
    
          return rndULongDivUInt(tLong, voltD);
    
       }
    
    /************************************************************
    * User API RTD Function Implementations
    ************************************************************/
    

    DAQAPI DaqError WINAPI
    daqCvtRtdConvert(PWORD counts, DWORD scans,
                     PSHORT temp, DWORD ntemp)
    {
    //   ********** Noise Averaging *********
    // To improve the temperature readings on the DBK 9 it may be
    // necessary to always average the Va and Vb (gains 0 & 1) readings
    // from the DBK9.  If Dbk9NoiseAvg is defined the daqRtdConvert will
    // always compute the average of Va and Vb over the all the scans that
    // are sent to it and then use the averaged values of Va and Vb for all
    // resitance (and temperature) calculations.  If Dbk9NoiseAvg is not
    // defined then all voltages will be used according to the specified
    // averaging method.
    // 7-28-95 It was decided to do the unconditional Va/Vb averaging for the first
    // release of the hardware.
    #define Dbk9NoiseAvg
    
    #define NumVoltReadings 4 //number of voltage readings per rtd
    #define VaOffset 0
    #define VbOffset 1
    //The third readings is not used in these calculations but it does
    //need to be in the scan sequence for the card to work correctly
    #define VdOffset 3
    
    unsigned rtdN, scanN;   //these are counters for the loops
    
    unsigned long vASum, vBSum, vDSum;
    
    //HJB 8/29/96 keep 16bit values
    WORD vAAvg, vBAvg, vDAvg;
    WORD resistance, baseIndex, curAvgs;
    
    //body of code here
    
       // parameter error checking
       if ( (!rtdSetup.nReadings) ||  (!rtdSetup.nRtd) ) { return apiCtrlProcessError(-1, DerrRtdNoSetup); }
       if (rtdSetup.avg) {
          // moving or single averaging
          if (ntemp < (rtdSetup.nRtd*scans) ) { return apiCtrlProcessError(-1, DerrRtdTArraySize); }
       } else {
          // block averaging
          if (ntemp < rtdSetup.nRtd ) { return apiCtrlProcessError(-1, DerrRtdTArraySize); }
       }
    
       // Convert the data here.
       switch (rtdSetup.avg) {

       case  0:
          // Block averaging
          for (rtdN = 0; rtdN<rtdSetup.nRtd; rtdN++) {
             vASum = 0;
             vBSum = 0;
             vDSum = 0;
             for (scanN=0; scanN<scans; scanN++) {
                baseIndex = (scanN * rtdSetup.nReadings) + rtdSetup.startPosition + (rtdN * NumVoltReadings);
                vASum += counts[baseIndex + VaOffset];
                vBSum += counts[baseIndex + VbOffset];
                vDSum += counts[baseIndex + VdOffset];
             }
             vAAvg = rndULongDivUInt(vASum, (WORD)scans);//rounding long divided
             vBAvg = rndULongDivUInt(vBSum, (WORD)scans);// by an integer
             vDAvg = rndULongDivUInt(vDSum, (WORD)scans);
    
             resistance = rtdCounts2Resistance(vAAvg, vBAvg, vDAvg);
             temp[rtdN] = rtdResistance2Temp(resistance);
          }
          break;
    
       case 1:
          // No averaging
          // For each scan
          for (rtdN = 0; rtdN<rtdSetup.nRtd; rtdN++) {
             #ifdef Dbk9NoiseAvg
                // get average vA vB
                vASum = 0;
                vBSum = 0;
                for (scanN=0; scanN<scans; scanN++) {
                   baseIndex = (scanN * rtdSetup.nReadings) + rtdSetup.startPosition + (rtdN * NumVoltReadings);
                   vASum += counts[baseIndex + VaOffset];
                   vBSum += counts[baseIndex + VbOffset];
                }
                vAAvg = rndULongDivUInt(vASum, (WORD)scans);

                vBAvg = rndULongDivUInt(vBSum, (WORD)scans);
             #endif
             // compute temperatures using averaged vA vB
             for (scanN=0; scanN<scans; scanN++) {
                baseIndex = (scanN * rtdSetup.nReadings) + rtdSetup.startPosition + (rtdN * NumVoltReadings);
                #ifndef Dbk9NoiseAvg
                   // **** commented out individual vA vB readings
                   vAAvg = counts[baseIndex + VaOffset];  // averages of one count
                   vBAvg = counts[baseIndex + VbOffset];
                #endif
                vDAvg = counts[baseIndex + VdOffset];
                resistance = rtdCounts2Resistance(vAAvg, vBAvg, vDAvg);
                temp[ (scanN * rtdSetup.nRtd) + rtdN ] = rtdResistance2Temp(resistance);
             }
          }
          break;
    
       default:
          // compute temperatures using averaged vA vB
          // Moving average
          for (rtdN = 0; rtdN<rtdSetup.nRtd; rtdN++) {
             curAvgs = 0;
             vASum = 0;
             vBSum = 0;
             #ifdef Dbk9NoiseAvg
                // get average vA vB
                for (scanN=0; scanN<scans; scanN++) {
                   baseIndex = (scanN * rtdSetup.nReadings) + rtdSetup.startPosition + (rtdN * NumVoltReadings);
                   vASum += counts[baseIndex + VaOffset];
                   vBSum += counts[baseIndex + VbOffset];
                }
                vAAvg = rndULongDivUInt(vASum, (WORD)scans);
                vBAvg = rndULongDivUInt(vBSum, (WORD)scans);
             #endif
             vDSum = 0;
             // for each scan
             for (scanN=0; scanN<scans; scanN++) {
                baseIndex = (scanN * rtdSetup.nReadings) + rtdSetup.startPosition + (rtdN * NumVoltReadings);
                // are we enough into the scan arrays to average 'rtdSetup.avg' scans
                if (curAvgs < rtdSetup.avg) {
                   // if not increment counter.  curAvgs will increment until it equals

                   //   the number of scans in the moving average block
                   curAvgs++;
                } else {
                   // subtract from the sums the count values of the scan
                   #ifndef Dbk9NoiseAvg
                      //    that is no longer in this moving average block
                      vASum -= counts[baseIndex + VaOffset - (curAvgs * rtdSetup.nReadings)];
                      vBSum -= counts[baseIndex + VbOffset - (curAvgs * rtdSetup.nReadings)];

                   #endif
                   vDSum -= counts[baseIndex + VdOffset - (curAvgs * rtdSetup.nReadings)];
                }
                // add in the counts from the current (new) scan
                #ifndef Dbk9NoiseAvg
                   vASum += counts[baseIndex + VaOffset];
                   vBSum += counts[baseIndex + VbOffset];
                #endif
                vDSum += counts[baseIndex + VdOffset];
    
                #ifndef Dbk9NoiseAvg
                   vAAvg = rndULongDivUInt(vASum, curAvgs);  //rounding errors??
                   vBAvg = rndULongDivUInt(vBSum, curAvgs);
                #endif
                vDAvg = rndULongDivUInt(vDSum, curAvgs);
    
                resistance = rtdCounts2Resistance(vAAvg, vBAvg, vDAvg);
                temp[ (scanN * rtdSetup.nRtd) + rtdN ] = rtdResistance2Temp(resistance);
             }
          }
       }
    
       return DerrNoError;
    
    }
    
  
    DAQAPI DaqError WINAPI
    daqCvtRtdSetup(DWORD nScan, DWORD startPosition, DWORD nRtd,
                 RtdType rtdValue, DWORD avg)
    {
       if ( (!nScan) || (!nRtd) ) { return apiCtrlProcessError(-1, DerrRtdParam); }
       if (rtdValue > Dbk9RtdType1K )  { return apiCtrlProcessError(-1, DerrRtdValue); }
    

       rtdSetup.nReadings      = nScan;
       rtdSetup.startPosition  = startPosition;
       rtdSetup.nRtd           = nRtd;
       rtdSetup.rtdValue       = rtdValue;
       rtdSetup.avg            = avg;
    
       return DerrNoError;
    
    }
    
 
    DAQAPI DaqError WINAPI
    daqCvtRtdSetupConvert(DWORD nScan, DWORD startPosition,
                       DWORD nRtd, RtdType rtdValue, DWORD avg, PWORD counts,
                       DWORD scans, PSHORT temp, DWORD ntemp)
    {
       // parameter checking done by setup & convert
       DaqError err;
       err = daqCvtRtdSetup(nScan, startPosition, nRtd, rtdValue, avg);
		if(err==DerrNoError) 
          err = daqCvtRtdConvert(counts, scans, temp, ntemp);
  
    
       return err;
    
    }
    
    /************************************************************
    * User API Auto Zero Function Implementations
    ************************************************************/

  
    DAQAPI DaqError WINAPI
    daqAutoZeroCompensate(DaqAutoZeroCompT zero)
    { /* $skip start$ */
    
	   switch(zero) {
         case DazcNone: 
		      session->zeroDbk19 = FALSE;
         break;
         default: 
		      session->zeroDbk19 = TRUE;
       }

       return DerrNoError;
    
      /* $skip end$ */
    } /* $end$ */
    

 
    DAQAPI DaqError WINAPI
    daqZeroDbk19(BOOL zero )
    { /* $skip start$ */
    
       session->zeroDbk19 = zero;
    
       return DerrNoError;
    
      /* $skip end$ */
    } /* $end$ */
    
    
  
    
    DAQAPI DaqError WINAPI 
    daqCvtSetAdcRange(float min, float max)
    {
      Admin = min;
      Admax = max;
    
      return DerrNoError;
    }
    
   
    float
    getY(float x, float x1, float y1, float x2, float y2)
    { /* $skip start$ */
    
       return ((y1 - y2) * x + x1 * y2 - x2 * y1) / (x1 - x2);
    
      /* $skip end$ */
    } /* $end$ */
    
  
    float
    linearCounts2Units(unsigned long cSum, unsigned scans)
    { /* $skip start$ */
    
       float cAvg, vAvg;
    
       // convert the count sum to a count average
       cAvg = (float)cSum / (float)scans;
    
       // convert the count average to a voltage average
       // FIX ME - assumes unsigned data, may have to redesign for
       // accomodating signed data
       vAvg = getY(cAvg,
                   0.0f, Admin,
                   65536.0f,Admax);
    
       // convert the voltage average to a units average and return
       return getY(vAvg,
                   LinearSetup.voltage1, LinearSetup.signal1,
                   LinearSetup.voltage2, LinearSetup.signal2);
    
      /* $skip end$ */
    } /* $end$ */
    
    
  
    DAQAPI DaqError WINAPI
    daqCvtLinearSetup(DWORD nscan, DWORD readingsPos, DWORD nReadings,
                      float signal1, float voltage1, float signal2, float voltage2,
                      DWORD avg)
    { /* $skip start$ */
    
       #ifdef DebugApiFunc
          DebugString("\ndaqLinearSetup");
       #endif
    
       if (
           (nscan==0                   ) ||
           (nscan>512                  ) ||
           (readingsPos>=nscan         ) ||
           (voltage1==voltage2         ) ||
           (nReadings==0               ) ||
           (nReadings+readingsPos>nscan)
          ) { return apiCtrlProcessError(-1,DerrZCInvParam); }
    
       LinearSetup.nscan = nscan;
       LinearSetup.readingsPos = readingsPos;
       LinearSetup.nReadings = nReadings;
       LinearSetup.signal1 = signal1;
       LinearSetup.voltage1 = voltage1;
       LinearSetup.signal2 = signal2;
       LinearSetup.voltage2 = voltage2;
       LinearSetup.avg = avg;
    
       return DerrNoError;
    
      /* $skip end$ */
    } /* $end$ */
    
  
    DAQAPI DaqError WINAPI
    daqCvtLinearConvert(PWORD counts, DWORD scans, float *  fValues, DWORD nValues)
    { /* $skip start$ */
    
       unsigned readingN, scanN;   //these are counters for the loops
       unsigned avgCount;
       WORD *countsPtr1, *countsPtr2;

       unsigned long countsSum;
       float *  pfValue;
    
       #ifdef DebugApiFunc
          DebugString("\ndaqLinearConvert");
       #endif
    
       // parameter error checking
       if (!LinearSetup.nscan) { return apiCtrlProcessError(-1,DerrZCNoSetup); }
       if (LinearSetup.avg) {
          // moving or single averaging
          if (nValues < (LinearSetup.nReadings * scans) ) { return apiCtrlProcessError(-1,DerrArraySize); }
       } else {
          // block averaging
          if (nValues < LinearSetup.nReadings ) { return apiCtrlProcessError(-1,DerrArraySize); }
       }
    
       // Convert the data here.
       switch (LinearSetup.avg) {
       case  0:
          // Block averaging
          pfValue = fValues;
          for (readingN = 0; readingN < LinearSetup.nReadings; readingN++) {
             countsPtr1 = &counts[LinearSetup.readingsPos + readingN];
             countsSum = 0;
             for (scanN = 0; scanN < scans; scanN++) {
               countsSum += *countsPtr1;
               countsPtr1 += LinearSetup.nscan;
             }
             *pfValue = linearCounts2Units(countsSum, scans);
             pfValue++;
          }
          break;
    
       default:
          // Moving or no average (no average (1) is a special case of moving)
          for (readingN = 0; readingN < LinearSetup.nReadings; readingN++) {
             pfValue = &fValues[readingN];
             countsPtr1 = &counts[LinearSetup.readingsPos + readingN];
             countsPtr2 = countsPtr1;
             avgCount = 0;
             countsSum = 0;
             for (scanN = 0; scanN < scans; scanN++) {
                if (avgCount < LinearSetup.avg) {
                   avgCount++;
                } else {
                   countsSum -= *countsPtr2;
                   countsPtr2 += LinearSetup.nscan;
                }
                countsSum += *countsPtr1;
                countsPtr1 += LinearSetup.nscan;
                *pfValue = linearCounts2Units(countsSum, avgCount);
                pfValue += LinearSetup.nReadings;
             }
         }
         break;
    
      }
    
       return DerrNoError;
    
      /* $skip end$ */
    } /* $end$ */
    

   
    DAQAPI DaqError WINAPI
    daqCvtLinearSetupConvert(DWORD nscan, DWORD readingsPos, DWORD nReadings,
                             float signal1, float voltage1, float signal2,
                             float voltage2, DWORD avg, PWORD counts, DWORD scans,
                             float *  fValues, DWORD nValues)
    { /* $skip start$ */
    
       DaqError err;
    
       #ifdef DebugApiFunc
          DebugString("\ndaqLinearSetupConvert");
       #endif
    
       err = daqCvtLinearSetup(nscan, readingsPos, nReadings, signal1,
                            voltage1, signal2, voltage2, avg);
       if (!err)
       {
          err = daqCvtLinearConvert(counts, scans, fValues, nValues);
       }
       return err;
    
      /* $skip end$ */
    } /* $end$ */
    
    
    #endif

 
    DAQAPI DaqError WINAPI
    daqZeroConvert(PWORD counts, DWORD scans)
    { /* $skip start$ */
    
       unsigned i, j;
       long l;
    
       #ifdef DebugApiFunc
          DebugString("\ndaqZeroConvert");
       #endif
    
       if (!zeroSetup.nscan) { return apiCtrlProcessError(-1,DerrZCNoSetup); }
       for (j=0; j<scans; j++) {
          for (i=0; i<zeroSetup.nReadings; i++) {
             l = counts[ j * zeroSetup.nscan + i + zeroSetup.readingsPos ];
             l -= counts[ j * zeroSetup.nscan + zeroSetup.zeroPos ];
             l += 0x8000;
             if ( l < 0 ) { l=0; }
             if ( l > 65535l ) { l=65535l; }
             counts[ j * zeroSetup.nscan + i + zeroSetup.readingsPos ] = (unsigned int)l;
          }
       }
    
       return DerrNoError;  
    
      /* $skip end$ */
    } /* $end$ */
    

  
    DAQAPI DaqError WINAPI
    daqZeroSetup(DWORD nscan, DWORD zeroPos, DWORD readingsPos,
                 DWORD nReadings)
    { /* $skip start$ */
    
       #ifdef DebugApiFunc
          DebugString("\ndaqZeroSetup");
       #endif
    
       if (
           (nscan==0                   ) ||
           (nscan>512                  ) ||
           (zeroPos>=nscan             ) ||
           (readingsPos>=nscan         ) ||
           (nReadings==0               ) ||
           (nReadings+readingsPos>nscan) ||
           ((zeroPos>=readingsPos) && (zeroPos<readingsPos+nReadings))
          ) { return apiCtrlProcessError(-1,DerrZCInvParam); }
    
       zeroSetup.nscan = nscan;
       zeroSetup.zeroPos = zeroPos;
       zeroSetup.readingsPos = readingsPos;
       zeroSetup.nReadings = nReadings;
    
       return DerrNoError;
    
      /* $skip end$ */
    } /* $end$ */
    

  
    DAQAPI DaqError WINAPI
    daqZeroSetupConvert(DWORD nscan, DWORD zeroPos,
                        DWORD readingsPos, DWORD nReadings, PWORD counts,
                        DWORD scans)
    { /* $skip start$ */
    
       DaqError err;
    
       #ifdef DebugApiFunc
          DebugString("\ndaqZeroSetupConvert");
       #endif
    
       err = daqZeroSetup(nscan, zeroPos, readingsPos, nReadings);
       if (!err)
       {
          err = daqZeroConvert(counts, scans);
       }
       return err;
    
      /* $skip end$ */
    } /* $end$ */

    /* used by daqCalcTCTriggerVals */
	int ExtTemp2Voltage(double *voltage, double TCtemp, TCType tcType)
    {
       const T2VT  *seg;
       int      i;
       double    temp;
	    const ThermocoupleT *localTCType;
	   
	   //tcType = tcSetup.tcType;
	   localTCType = thermocouple[tcType];

	   temp = TCtemp;
                  
	   if (temp < localTCType->t2v[0].startTemp) { return apiCtrlProcessError(-1, DerrTCE_VRANGE); }
    
       for (i=0; i < localTCType->nt2v; ++i)
          {
          if (temp <= localTCType->t2v[i].endTemp) { break; }
          }
       if (! (i < localTCType->nt2v)) { return apiCtrlProcessError(-1, DerrTCE_VRANGE); }
    
       seg = &localTCType->t2v[i];
    
       Poly(temp, seg->ncoeff - 1,(CoeffT *)seg->coeff);
	   
	   *voltage = result;
	   
	   DebugWinString2("TC Trigger => Temp = %6.2f : milliVolts = %6.3f\r\n",temp,result*1000);
       return 0;
	}
