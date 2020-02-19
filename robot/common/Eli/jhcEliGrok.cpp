// jhcEliGrok.cpp : post-processed sensors and innate behaviors for ELI robot
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

#include "Interface/jms_x.h"           // common video
#include "Interface/jrand.h"

#include "Eli/jhcEliGrok.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcEliGrok::~jhcEliGrok ()
{
}


//= Default constructor initializes certain values.
// creates member instances here so header file has fewer dependencies

jhcEliGrok::jhcEliGrok ()
{
  // connect head tracker to other pieces
  fn.Bind(&s3);
  tk.Bind(&s3);

  // no body yet
  clr_ptrs();

  // change some head finder/tracker defaults
  s3.SetMap(192.0, 96.0, 96.0, 0.0, 20.0, 84.0, 0.3, 0.0);   // 16' wide x 8' front
  s3.ch = 34.0;                                              // allow seated
  s3.h0 = 40.0;
  s3.ring = 120.0;                                           // long range okay
  s3.edn = 5.0;

  // change some face finder/gaze defaults
  fn.xsh = 0.4;                                              // big search box
  fn.ysh = 0.4;
  fn.dadj = 2.0;                                             // head is shell

  // information about watching behavior
  src.SetSize(4);
  wtarg[8][0] = '\0';
  strcpy_s(wtarg[7], "<frozen>");
  strcpy_s(wtarg[6], "speaker");
  strcpy_s(wtarg[5], "SOUND");
  strcpy_s(wtarg[4], "closest person");
  strcpy_s(wtarg[3], "eye contact");
  strcpy_s(wtarg[2], "recent face");
  strcpy_s(wtarg[1], "twitch");
  strcpy_s(wtarg[0], "<neutral>");
}


//= Attach extra processing to physical or simulated body.

void jhcEliGrok::BindBody (jhcEliBody *b)
{
  const jhcImg *src;

  // possible unbind body and pieces
  clr_ptrs();
  if (b == NULL)
    return;

  // make direct pointers to body parts (for convenience)
  // and use voice tracker mic for speaker direction
  body = b;
  base = &(b->base);
  neck = &(b->neck);
  lift = &(b->lift);
  arm  = &(b->arm);
  mic  = &(b->mic);
  tk.RemoteMic(mic);         
  
  // configure visual analysis for camera images
  src = body->View();
  s3.SetSize(*src);
}


//= Null pointers to body and subcomponents.

void jhcEliGrok::clr_ptrs ()
{
  body = NULL;
  base = NULL;
  neck = NULL;
  lift = NULL;
  arm  = NULL;
  mic  = NULL;
  tk.RemoteMic(NULL);
}


//= Generate a string telling what the robot is paying attention to.

const char *jhcEliGrok::Watching () const
{
  int bid[8] = {neutral, twitch, face, stare, close, sound, speak, freeze};
  int i, win;

  // see which bid value won last command arbitration  
  if (body == NULL)
    return NULL;
  win = neck->GazeWin();
  if (win <= 0)
    return wtarg[8];
  
  // determine behavior by checking against standard bids (if non-zero)
  for (i = 0; i < 8; i++)
    if (win == bid[i])
      return wtarg[i];
  return wtarg[8];
}


///////////////////////////////////////////////////////////////////////////
//                         Processing Parameters                         //
///////////////////////////////////////////////////////////////////////////

//= Parameters controlling what sort of activities to watch.

int jhcEliGrok::watch_params (const char *fname)
{
  jhcParam *ps = &wps;
  int ok;

  ps->SetTag("rwi_watch", 0);
  ps->NextSpec4( &freeze,  27,   "Post-cmd freeze bid");
  ps->NextSpec4( &speak,   26,   "Current speaker bid");
  ps->NextSpec4( &sound,   25,   "Most recent sound bid");
  ps->NextSpec4( &close,   24,   "Closest head bid");
  ps->NextSpec4( &stare,   23,   "Most recent stare bid");
  ps->NextSpec4( &face,    22,   "Most recent face bid");

  ps->NextSpec4( &twitch,  21,   "Random gaze bid");
  ps->NextSpec4( &neutral,  0,   "Reset neck bid");          // was 20
  ok = ps->LoadDefs(fname);
  ps->RevertAll();
  return ok;
}


//= Parameters used for picking targets to watch.

int jhcEliGrok::orient_params (const char *fname)
{
  jhcParam *ps = &ops;
  int ok;

  ps->SetTag("rwi_orient", 0);
  ps->NextSpecF( &bored, 10.0, "Post-cmd freeze (sec)"); 
  ps->NextSpecF( &edge,  30.0, "Sound trigger offset (deg)");
  ps->NextSpecF( &hnear, 72.0, "Head near start (in)");
  ps->NextSpecF( &hfar,  80.0, "Head far finish (in)");
  ps->NextSpec4( &fmin,   3,   "Min face detections");
  ps->NextSpecF( &dwell,  1.5, "Target dwell time (sec)");
  
  ps->NextSpecF( &side,  50.0, "Body rotate thresh (deg)");   // 0 = don't
  ps->NextSpecF( &tfix,  30.0, "Body rotate goal (deg)");  
  ok = ps->LoadDefs(fname);
  ps->RevertAll();
  return ok;
}


//= Parameters controlling idle activities.

int jhcEliGrok::idle_params (const char *fname)
{
  jhcParam *ps = &ips;
  int ok;

  ps->SetTag("rwi_idle", 0);
  ps->NextSpecF( &center,  1.0, "Twitch start stable (deg)");
  ps->NextSpecF( &aim,    30.0, "Max twitch offset (deg)");
  ps->NextSpecF( &relax,   7.0, "Twitch interval (sec)");  
  ps->NextSpecF( &rdev,    3.0, "Twitch deviation (sec)");
  ps->NextSpecF( &pdist,  36.0, "Default person dist (in)");
  ps->NextSpecF( &pht,    52.0, "Default person ht (in)");
  ok = ps->LoadDefs(fname);
  ps->RevertAll();
  return ok;
}


///////////////////////////////////////////////////////////////////////////
//                           Parameter Bundles                           //
///////////////////////////////////////////////////////////////////////////

//= Read all relevant defaults variable values from a file.

int jhcEliGrok::Defaults (const char *fname)
{
  int ok = 1;

  ok &= watch_params(fname);
  ok &= orient_params(fname);
  ok &= idle_params(fname);
  ok &= fn.Defaults(fname);      // does s3 also
  return ok;
}


//= Read just deployment specific values from a file.

int jhcEliGrok::LoadCfg (const char *fname)
{
  int ok = 1;

  if (body != NULL)
    ok &= body->Defaults(fname);
  return ok;
}


//= Write current processing variable values to a file.

int jhcEliGrok::SaveVals (const char *fname) 
{
  int ok = 1;

  ok &= wps.SaveVals(fname);
  ok &= ops.SaveVals(fname);
  ok &= ips.SaveVals(fname);
  ok &= fn.SaveVals(fname);      // does s3 also
  return ok;
}


//= Write current deployment specific values to a file.

int jhcEliGrok::SaveCfg (const char *fname) const
{
  int ok = 1;

  if (body != NULL)
    ok &= body->SaveVals(fname);
  fn.SaveCfg(fname);
  return ok;
}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Restart background processing loop.
// NOTE: body should be reset outside of this

void jhcEliGrok::Reset ()
{
  // reset vision components
  s3.Reset();
  fn.Reset();

  // after body reset, sensor info will be waiting and need to be read
  if (body != NULL)
  {
    body->InitPose(-1.0);
    body->Update(-1, 1);
    body->BigSize(mark);
    mark.FillArr(0);
  }

  // clear targets for watching behaviors
  twin = -1;
  hwin = -1;
  gwin = -1;
  fwin = -1;

  // clear state for watching behaviors
  seek  = 0;
  rwait = 0;  
  idle  = 0;

  // restart background loop, which first generates a body Issue call
  jhcBackgRWI::Reset();
}


//= Read and process all sensory information from robot.
// this all happens when background thread in rwi update is quiescent
// returns 1 if okay, 0 or negative for error

int jhcEliGrok::Update (int voice, UL32 resume)
{
  // do slow vision processing in background (already started usually)
  if (jhcBackgRWI::Update(0) <= 0)
    return 0;

  // do fast sound processing in foreground (needs voice)
  mic->Update(voice);
  tk.Analyze(voice);

  // create pretty picture then enforce min wait (to simulate robot)
  interest_img();
  jms_resume(resume);  
  return 1;
}


//= Call at end of main loop to stop background processing and robot motion.

void jhcEliGrok::Stop ()
{
  jhcBackgRWI::Stop();
  if (body != NULL)
    body->Limp();
}


///////////////////////////////////////////////////////////////////////////
//                          Interaction Overrides                        //
///////////////////////////////////////////////////////////////////////////

//= Run local behaviors then send arbitrated commands to body.

void jhcEliGrok::body_issue ()
{
  // record current time
  tnow = jms_now();

  // run some reactive behaviors (tk is up-to-date) 
  cmd_freeze();
  watch_talker();
  gaze_sound();
  watch_closest();
  gaze_stare();
  gaze_face();
  gaze_random();
  head_neutral();

  // start commands and get new raw images
  body->Issue();
  seen = body->UpdateImgs();         
}


//= Get sensor inputs and fully process images.

void jhcEliGrok::body_update ()
{
  jhcMatrix pos(4), dir(4);

  // wait (if needed) for body sensor data to be received (no mic)
  body->Update(-1, 0);

  // do slow head finding and tracking (needs both pose and image)
  if (seen > 0)
  {  
    neck->HeadPose(pos, dir, lift->Height());
    fn.SetCam(pos, dir);
    fn.Analyze(body->Color(), body->Range());
  }
}


///////////////////////////////////////////////////////////////////////////
//                            Innate Behaviors                           //
///////////////////////////////////////////////////////////////////////////

//= Freeze head and feet if recent conscious command issued.

void jhcEliGrok::cmd_freeze ()
{
  if (freeze <= 0)
    return;
  if (body->NeckIdle(tnow) <= bored)
    neck->ShiftTarget(0.0, 0.0, 1.0, freeze);
  if (body->BaseIdle(tnow) <= bored)
    base->DriveTarget(0.0, 0.0, 1.0, freeze);
}


//= Continuously look at whomever is currently talking (if anyone).

void jhcEliGrok::watch_talker ()
{
  if (speak <= 0)
    return;
  set_target(twin, twait, tk.Speaking(), 1);
  orient_toward(s3.GetID(twin), speak);
}


//= Look at non-central sound source (if any) for a while.
// state machine controlled by "seek" and timer "swait"

void jhcEliGrok::gaze_sound ()
{
  double ang, rads;
  int old = 1;

  // see if behavior desired 
  if (sound <= 0)
    return;

  // trigger behavior when sound is far to either side
  if (mic->VoiceStale() <= 0)
  {
    ang = mic->VoiceDir();
    if (fabs(ang) >= edge)
    {
      // remember location since sound is often short duration
      rads = D2R * (ang + 90.0);
      src.SetVec3(pdist * cos(rads), pdist * sin(rads), pht);
      old = 0;
      seek = 1;
      swait = tnow;
    }
  }
  if (seek <= 0)
    return;

  // adjust for any base motion then aim at remembered location
  if (old > 0)
    base->AdjustTarget(src);   
  orient_toward(&src, sound);
  if (jms_diff(tnow, swait) >= ROUND(1000.0 * dwell))
    seek = 0;
}


//= Track the closest head (even if face not visible).

void jhcEliGrok::watch_closest ()
{
  const jhcMatrix *hd;
  double d;
  int old = hwin;

  // see if behvaior desired then find closest head
  if (close <= 0)
    return;
  if ((hwin = s3.Closest()) < 0)
    return;
  hd = s3.GetPerson(hwin);

  // follow if planar distance within hysteretic bounds
  d = hd->PlaneVec3();
  if ((d <= hnear) || ((hwin == old) && (d <= hfar)))
    orient_toward(hd, close);
  else
    hwin = -1;                         // stop tracking
}


//= Look at most recent person staring at robot (if any).

void jhcEliGrok::gaze_stare ()
{
  if (stare <= 0)
    return;
  set_target(gwin, gwait, fn.GazeNew());
  orient_toward(s3.GetPerson(gwin), stare);
}


//= Look a while at most recently found face (if any),

void jhcEliGrok::gaze_face ()
{
  if (face <= 0)
    return;
  set_target(fwin, fwait, fn.FrontNew(0, fmin)); 
  orient_toward(s3.GetPerson(fwin), face);
}


//= Flick gaze toward a random position after a while.
// state machine controlled by "delay" and timers "idle" and "rwait"
// <pre>
//   rwait    idle      state
//     0        0       get new inter-twitch
//     0        x       wait until twitch
//     x        0       twitch for a while
//     x        x       --never occurs--
// </pre>

void jhcEliGrok::gaze_random ()
{
  jhcMatrix hd(4), cam(4), dir(4);
  double pan, tilt;

  // see if behavior desired
  if (twitch <= 0)
    return;

  // if not currently twitching
  if (rwait == 0)
  {
    // possibly pick a new time for next twitch (once)           
    if (idle == 0) 
    {
      delay = ROUND(1000.0 * jrand_cent(relax, rdev));
      delay = __max(1, delay);
      neck->Gaze(prand, trand);
      idle = tnow;                         // advance to next state
      return;
    }

    // reset inter-twitch timer if head moved too much
    if (jms_diff(tnow, idle) < delay)
    {
      if (neck->GazeErr(prand, trand) > center)
        idle = tnow;
      neck->Gaze(prand, trand);
      return;
    }

    // get angles to likely forward head 
    hd.SetVec3(0.0, pdist, pht);
    neck->HeadPose(cam, dir, lift->Height());
    cam.PanTilt3(pan, tilt, hd);
    pan -= 90.0;                             

    // perturb by some random amount (once)
    prand = pan  + jrand_cent(0.0, aim);
    trand = tilt + jrand_cent(0.0, aim);
    rwait = tnow;                          // advance to next state
    idle = 0;
  }

  // if time not expired then pursue target, otherwise reset state machine 
  if (jms_diff(tnow, rwait) < ROUND(1000.0 * dwell))
    neck->GazeTarget(prand, trand, 1.0, 0.0, twitch);
  else
    rwait = 0;                             // advance to next state
}


//= Set default gaze toward expected head of person straight ahead.

void jhcEliGrok::head_neutral ()
{
  jhcMatrix hd(4);

  if (neutral <= 0)
    return;
  hd.SetVec3(0.0, pdist, pht);
  orient_toward(&hd, neutral);
}


//= Accept proposed target if valid, else update dwell timer.
// invalidates target after temporal extension expires

void jhcEliGrok::set_target (int& targ, UL32& timer, int i, int th) const
{
  if (i >= th)
  {
    targ = i;
    timer = tnow;
  }
  else if (jms_diff(tnow, timer) >= ROUND(1000.0 * dwell))
    targ = -1;
}


//= Aim camera at target location, rotating body if needed.
// set "turn" to zero or negative to prevent body rotation

void jhcEliGrok::orient_toward (const jhcMatrix *targ, int bid)
{
  jhcMatrix cam(4), dir(4);
  double pan, tilt, sp = 1.0;

  // slower speed for some behaviors
  if ((bid == neutral) || (bid == twitch))
    sp = 0.5;

  // figure out pan and tilt angles to target (forward = 90 degs)
  if (targ == NULL) 
    return;
  neck->HeadPose(cam, dir, lift->Height());
  cam.PanTilt3(pan, tilt, *targ);
  pan -= 90.0;                             

  // make head point at given angles (only azimuth for sound)
  if (bid == sound)
    neck->PanTarget(pan, sp, bid);
  else
    neck->GazeTarget(pan, tilt, sp, 0.0, bid);

  // also move body if head turned a lot
  if (side > 0.0) 
  {
    if (pan > side)
      base->TurnTarget(pan - tfix, 1.0, bid);  
    else if (pan < -side)
      base->TurnTarget(pan + tfix, 1.0, bid);
  }
}


///////////////////////////////////////////////////////////////////////////
//                           Debugging Graphics                          //
///////////////////////////////////////////////////////////////////////////

//= Make a pretty version of color image showing relevant items.

void jhcEliGrok::interest_img ()  
{
  if ((body == NULL) || !(body->NewFrame()))
    return;
  body->ImgBig(mark);
  s3.HeadsCam(mark);                               // magenta - heads
  s3.ShowIDCam(mark, tk.Speaking(), 0, 1, 0, 2);   // green   - speaking
  fn.FacesCam(mark);                               // cyan    - faces
  fn.GazeCam(mark, fn.GazeNew());                  // yellow  - gaze
}


