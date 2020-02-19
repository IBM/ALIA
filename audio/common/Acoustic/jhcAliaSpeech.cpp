// jhcAliaSpeech.cpp : speech and loop timing interface for ALIA reasoner
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 IBM Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
///////////////////////////////////////////////////////////////////////////

#include "Acoustic/jhcAliaSpeech.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcAliaSpeech::~jhcAliaSpeech ()
{
  // for debugging - only happens when program closes
  DumpSession();
  DumpAll();
}


//= Default constructor initializes certain values.

jhcAliaSpeech::jhcAliaSpeech ()
{
  strcpy_s(gram,  "language/alia_top.sgm");    // main grammar file
  strcpy_s(kdir,  "KB/");                      // for kernels
  strcpy_s(kdir2, "KB2/");                     // extra abilities
  time_params(NULL);
  acc = 0;                                     // accumulate knowledge
}


///////////////////////////////////////////////////////////////////////////
//                         Processing Parameters                         //
///////////////////////////////////////////////////////////////////////////

//= Parameters used for overall control of timing.
// this should be called in Defaults and tps used in SaveVals

int jhcAliaSpeech::time_params (const char *fname)
{
  jhcParam *ps = &tps;
  int ok;

  ps->SetTag("asp_time", 0);
  ps->NextSpec4( &amode,    2,   "Attn (none, any, front, only)");
  ps->NextSpecF( &stretch,  2.5, "Attention window (sec)");  
  ps->NextSpec4( &wait,    12,   "Text out delay (cyc)");
  ps->Skip();
  ps->NextSpecF( &thz,     80.0, "Thought cycle rate (Hz)");  
  ps->NextSpecF( &shz,     30.0, "Default body rate (Hz)");  
  ok = ps->LoadDefs(fname);
  ps->RevertAll();
  return ok;
}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Reset state for the beginning of a sequence.
// speech: 0 for none, 1 for TTS only, 2 for reco/TTS, 3 for attn word
// can also set up robot name as an attention word
// returns 1 if okay, 0 or negative for error

int jhcAliaSpeech::Reset (int speech, const char *rname, const char *vname)
{
  char fname[200];
  int ans;

  // remember interface choice and set attentional state
  voice = speech;
  awake = 0;
  if (voice >= 2)
  {
    // constrain speech by same grammar as core
    sp.SetGrammar(gram); 
    ans = sp.Init(1, 0);                       // show partial transcriptions
    if (noisy > 0)
    {
      sp.PrintCfg();
      jprintf("SPEECH -> %s\n", ((ans > 0) ? "OK" : "FAILED !!!"));
      jprintf("=========================\n\n");
    }
    if (ans <= 0)
      return 0;

    // add kernel terms and robot name as attention word (speech only)
    kern_gram();
    self_name(rname);
    sp.MarkRule("toplevel");          
    sp.Listen(1);
  }
  else
    sp.InitTTS();                              // for echoing

  // set TTS and speech state
  if ((voice > 0) && (vname != NULL) && (*vname != '\0'))
    sp.SetVoice(vname);
  sp.Reset();

  // set basic grammar for core and clear state (speech already set)
  jprintf("Initializing ALIA core %4.2f\n\n", Version());
  MainGrammar(gram, "toplevel", rname);
  jhcAliaCore::Reset(1, rname);

  // load rules, operators, and words for kernels (speech already set)
  KernExtras(kdir);
  sprintf_s(fname, "%sbaseline.lst", kdir2);
  Baseline(fname, 1, 2);
  if (acc > 0)
    LoadLearned();

  // clear text input and output buffers
  *lastin = '\0';
  *input = '\0';
  done = 0;
  *output = '\0';
  *pend = '\0';
  yack = 0;

  // reset loop timing
  start = 0;
  rem = 0.0;
  sense = 0;
  think = 0;

  // suppress some printouts
  noisy = 1;
  attn.noisy = 1;

  // note that system is awake
  attn.StartNote();
  attn.AddProp(attn.self, "hq", "awake");
  attn.FinishNote();
  return 1;
}


//= Load speech system with extra grammar pieces associated with kernels.

void jhcAliaSpeech::kern_gram ()
{
  const jhcAliaKernel *k = &kern;
  const char *tag; 

  while (k != NULL)
  {
    tag = k->BaseTag();
    if (*tag != '\0')
      sp.LoadGrammar("%s%s.sgm", kdir, tag);
    k = k->NextPool();
  }
  sp.Listen(1);
}


//= Add the robot's own name as an attention word.

void jhcAliaSpeech::self_name (const char *name)
{
  char first[80];
  char *sep;

  if ((name == NULL) || (*name == '\0'))
    return;
  sp.ExtendRule("ATTN", name, 0); 
  strcpy_s(first, name);
  if ((sep = strchr(first, ' ')) != NULL)
  {
    *sep = '\0';
    sp.ExtendRule("ATTN", first, 0); 
  }
}


//= Initialize just the speech component for use with remote ALIA brain.

int jhcAliaSpeech::VoiceInit ()
{
  sp.SetGrammar(gram);      
  if (sp.Init(0, noisy) <= 0)
    return 0;
  sp.MarkRule("toplevel");    
  sp.Reset();
  kern_gram();      
  return 1;
}


//= Just do basic speech recognition (no reasoning) for debugging.
// returns 1 if happy, 0 to end interaction 

int jhcAliaSpeech::UpdateSpeech ()
{
  // wait for new sensors then check for new input (or exit)
  if (done > 0)
    return 0;
  sp.Update(voice - 1);
  if (voice >= 2)
    if (sp.Escape())
      return 0;
  return 1;
}


//= Generate actions in response to update sensory information.
// if "alert" > 0 then forces system to pay attention to input
// returns 1 if happy, 0 to end interaction 
// NOTE: should call UpdateSpeech before this and DayDream after this

int jhcAliaSpeech::Respond (int alert)
{
  int bid;

  // possibly wake up system then evaluate any language input
  now = jms_now();
  if (alert > 0)
    awake = now;
  xfer_input();

  // process current foci to generate commands for body
  RunAll();
  bid = Response(output);

  // send commands to body and language output channel (no delay)
  if (voice > 1)
  {
    sp.Say(bid, output);
    sp.Issue();
  }
  return 1;
}


//= If grammatical utterance then show parse and network.
// expects member variable "now" to hold current time
// state retained in member variable "wake" (timestamp of last activity)

void jhcAliaSpeech::xfer_input ()
{
  const char *sent;
  int hear, attn = ((voice >= 3) ? awake : 1);

  // get language status and input string 
  if (voice > 1)
  {
    hear = sp.Hearing();
    sent = sp.Heard();
  }
  else  
  {
    hear = ((*input != '\0') ? 2 : 0);
    sent = input;
  }

  // pass input (if any) to semantic network generator
  if (hear < 0)
    Interpret(NULL, attn, amode);                         // for "huh?" response
  else if (hear >= 2)
    if (Interpret(sent, attn, amode) >= 2)
      awake = now;

  // see if system should continue paying attention
  if (awake != 0) 
  {
    if (sp.Talking() > 0)                                 // prolong during output
      awake = now;
    else if ((attn > 0) && (sp.Silence() > 0.1) &&                     
             (jms_diff(now, awake) > ROUND(1000.0 * stretch)))
    {
      jprintf(1, noisy, "\n  ... timeout ... attention off\n");
      awake = 0;
    }
  }

  // percolate saved input strings
  strcpy_s(lastin, input);
  *input = '\0';
}


//= Perform several cycles of reasoning disconnected from sensors and actuators.

void jhcAliaSpeech::DayDream ()
{
  double frac;
  int i, cyc = 1;

  // determine how many total thought cycles to run
  if (start == 0)
    start = now;
  else 
  {
    frac = 0.001 * thz * jms_diff(now, last) + rem;
    cyc = ROUND(frac);
    rem = frac - cyc;
  }
  last = now;

  // possibly catch up on thinking (commands will be ignored)
  for (i = cyc - 1; i > 0; i--)
    RunAll();
  think += __max(1, cyc);
  sense++;
}


//= Call at end of run to put robot in stable state and possibly save knowledge.

void jhcAliaSpeech::Done ()
{
  if (voice > 1)
    sp.Listen(0);
  if (acc > 0)
    DumpLearned();
}


///////////////////////////////////////////////////////////////////////////
//                            Intercepted I/O                            //
///////////////////////////////////////////////////////////////////////////

//= Force a string into the input buffer.
// returns true if some valid input available for processing

bool jhcAliaSpeech::Accept (const char *in, int quit) 
{
  if (voice > 1)
    sp.Inject(in, quit); 
  else
  {
    if (in == NULL)
      *input = '\0';
    else
      strcpy_s(input, in);
    done = quit;
  }
  return((in != NULL) && (*in != '\0') && (quit <= 0));
}


//= Show input received on last cycle.
// if parsable, returns cleaned up version

const char *jhcAliaSpeech::NewInput ()
{
  const char *last, *fix;

  // get last input string and proper parse (if any)
  if (voice > 1)
    last = sp.LastIn();
  else
    last = lastin;
  fix = gr.Clean();

  // show input even if not parsed correctly
  if ((last == NULL) || (*last == '\0') || (awake == 0))
    return NULL;
  if ((fix == NULL) || (*fix == '\0'))          
    return last;
  return fix;
}


//= Show output completed on last cycle (delays text for "typing").

const char *jhcAliaSpeech::NewOutput () 
{
  const char *msg = NULL;
  UL32 now;

  // see if last output delayed long enough or interrupted
  now = jms_now();
  if (*pend != '\0')
  {
    if (jms_diff(now, yack) > ROUND(1000.0 * wait / thz))
      msg = blip_txt(0);
    else if (*output != '\0')
      msg = blip_txt(1);
  }

  // queue any new output for printing after slight delay
  if (*output != '\0')
  {
    strcpy_s(pend, output);
    yack = now;
    if (voice == 1)
    {
      // start TTS immediately but allow for later override
      sp.Say(sense, output);               
      sp.Utter();
    }
  }
  return msg;
}


//= Possibly terminate message after first word by inserting ellipsis.
// copies "pend" string to ephemeral "delayed" variable and erases "pend"

const char *jhcAliaSpeech::blip_txt (int cutoff)
{
  char *end;

  if (cutoff > 0)
  {
    if ((end = strchr(pend, ' ')) != NULL)
      *end = '\0';
    strcat_s(pend, " ...");
  }
  strcpy_s(delayed, pend);
  *pend = '\0';
  return delayed;
}


