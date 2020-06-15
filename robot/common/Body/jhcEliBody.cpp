// jhcEliBody.cpp : all mechanical aspects of Eli Robot (arm, neck, base, lift)
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2011-2020 IBM Corporation
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

#include <stdio.h>

#include "Interface/jhcMessage.h"
#include "Video/jhcKinVSrc.h"

#include "Body/jhcEliBody.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcEliBody::~jhcEliBody ()
{
  if (kin > 0)
    delete vid;
}


//= Default constructor initializes certain values.

jhcEliBody::jhcEliBody ()
{
  // shared Dynamixel serial port exists but is not open yet
  mok = -1;
  arm.Bind(&dyn);
  neck.Bind(&dyn);
  acc.Bind(&dyn);

  // starting values
  iw = 640;
  ih = 480;
  cw = 1280;
  ch = 960;
  kin = 0;
  vid = NULL;
  bnum = -1;

  // default robot name
  strcpy_s(rname, "hey you");
  *vname = '\0';
  loud = 0;

  // get standard processing values
  LoadCfg();
  Defaults();
  mic.SetGeom(0.0, 0.9, 44.5);         // position of mic (wrt wheel centers)
}


///////////////////////////////////////////////////////////////////////////
//                         Processing Parameters                         //
///////////////////////////////////////////////////////////////////////////

//= Parameters used for shared robot properties.
// nothing geometric that differs between bodies

int jhcEliBody::body_params (const char *fname)
{
  jhcParam *ps = &bps;
  int ok;

  ps->SetTag("body_cfg", 0);
  ps->NextSpec4( &dport,       5, "Dynamixel serial port");  
  ps->NextSpec4( &dbaud, 1000000, "Dynamixel baud rate");  
  ps->NextSpec4( &mega,        1, "Use AX-12 mega-update");
  ps->NextSpec4( &id0,         2, "Lowest mega-update ID");
  ps->NextSpec4( &idn,        11, "Highest mega-update ID");

  ok = ps->LoadDefs(fname);
  ps->RevertAll();
  return ok;
}


//= Parameters used for determine which resources are idle.
// nothing geometric that differs between bodies

int jhcEliBody::idle_params (const char *fname)
{
  jhcParam *ps = &ips;
  int ok;

  ps->SetTag("body_idle", 0);
  ps->NextSpec4( &nbid, 1000, "Neck busy bid");
  ps->NextSpec4( &lbid, 1000, "Lift busy bid");
  ps->NextSpec4( &abid, 1000, "Arm busy bid");
  ps->NextSpec4( &gbid, 1000, "Grip busy bid");
  ps->NextSpec4( &tbid, 1000, "Turn busy bid");
  ps->NextSpec4( &mbid, 1000, "Move busy bid");
  ok = ps->LoadDefs(fname);
  ps->RevertAll();
  return ok;
}


//= Read all relevant defaults variable values from a file.

int jhcEliBody::Defaults (const char *fname)
{
  int ok = 1;

  ok &= body_params(fname);
  ok &= idle_params(fname);
  ok &= arm.Defaults(fname);
  ok &= neck.Defaults(fname);
  ok &= base.Defaults(fname);
  ok &= lift.Defaults(fname);
  ok &= mic.Defaults(fname);
  ok &= acc.Defaults(fname);
  return ok;
}


//= Read just body specific values from a file.

int jhcEliBody::LoadCfg (const char *fname)
{
  char tail[80];
  const char *num;
  char *end;
  int ok = 1;

  // possibly set or change body number from file name
  if (fname != NULL)
    if ((num = strrchr(fname, '-')) != NULL)
    {
      strcpy_s(tail, num + 1);
      if ((end = strchr(tail, '.')) != NULL)
        *end = '\0';
      sscanf_s(tail, "%d", &bnum);
    }

  // get robot's default name and TTS voice
  bps.LoadText(rname, fname, "robot_name");
  bps.LoadText(vname, fname, "voice");
  if ((end = strchr(vname, '@')) != NULL)
  {
    // strip and save loudness adjustment (if any)
    sscanf_s(end + 1, "%d", &loud);
    *(end - 1) = '\0';
  }
  
  // load configuration for all components
  ok &= arm.LoadCfg(fname);
  ok &= neck.LoadCfg(fname);
  ok &= base.LoadCfg(fname);
  ok &= lift.LoadCfg(fname);
  ok &= mic.LoadCfg(fname);
  ok &= acc.LoadCfg(fname);
  return ok;
}


//= Write current processing variable values to a file.

int jhcEliBody::SaveVals (const char *fname) const
{
  int ok = 1;

  ok &= bps.SaveVals(fname);
  ok &= ips.SaveVals(fname);
  ok &= arm.SaveVals(fname);
  ok &= neck.SaveVals(fname);
  ok &= base.SaveVals(fname);
  ok &= lift.SaveVals(fname);
  ok &= mic.SaveVals(fname);
  ok &= acc.SaveVals(fname);
  return ok;
}


//= Write current body specific values to a file.

int jhcEliBody::SaveCfg (const char *fname) const
{
  char full[80];
  int ok = 1;

  // save robot's default name and TTS voice
  bps.SaveText(fname, "robot_name", rname);
  if ((loud <= 0) || (loud >= 100))
    bps.SaveText(fname, "voice", vname);
  else
  { 
    sprintf_s(full, "%s @%d", vname, loud);
    bps.SaveText(fname, "voice", full);
  }

  // save configuration for all components
  ok &= arm.SaveCfg(fname);
  ok &= neck.SaveCfg(fname);
  ok &= base.SaveCfg(fname);
  ok &= lift.SaveCfg(fname);
  ok &= mic.SaveCfg(fname);
  ok &= acc.SaveCfg(fname);
  return ok;
}


///////////////////////////////////////////////////////////////////////////
//                               Configuration                           //
///////////////////////////////////////////////////////////////////////////

//= Bind an external video source to be used.

void jhcEliBody::BindVideo (jhcVideoSrc *v)
{
  if (kin > 0)
    delete vid;
  kin = 0;
  vid = v;
  chk_vid(0);
}


//= Bind the Kinect depth sensor for obtain video and range.

int jhcEliBody::SetKinect (int rpt)
{
  jhcKinVSrc *k;

  // make sure not already bound
  if (kin > 0)
    return 1;

  // try connecting
  if (rpt > 0)
    jprintf("Initializing depth sensor ...\n");
  if ((k = new jhcKinVSrc("0.kin")) == NULL)
  {
    if (rpt >= 2)
      Complain("Could not communicate with Kinect");
    else if (rpt > 0)
      jprintf(">>> Could not communicate with Kinect !\n");
    return 0;
  }

  // configure images
  if (rpt > 0)
    jprintf("    ** good **\n\n");
  BindVideo(k);
  kin = 1;
  return 1;
}


//= Reset state for the beginning of a sequence.
// needs to be called at least once before using robot
// if rpt > 0 then prints to log file
// if full > 0 then clears all communications and tests hardware

int jhcEliBody::Reset (int rpt, int full) 
{
  char fname[80];
  UL32 neg5 = jms_now() - 300000;      // idle 5 minutes
  int i;

  if ((full > 0) || (CommOK(0) <= 0))
  {
    // announce entry
    if (rpt > 0)
    {
      jprintf("=========================\n");
      jprintf("BODY reset ...\n");
    }

    // possibly load configuration for a particular body
    if (CfgFile(fname, 1, 80) > 0)
    {
      if (rpt > 0)
        jprintf("  loading configuration for robot %d ...\n", __max(0, bnum));
      LoadCfg(fname);
    }

    // connect to proper serial port (if needed) 
    if (mok < 0)
    {
      if (dyn.SetSource(dport, dbaud, 256) > 0)
        mok = 1;
      else if (rpt >= 2)
        Complain("Could not open Dynamixel serial port %d in jhcEliBody::Reset", dport);
      else if (rpt > 0)
        jprintf(">>> Could not open Dynamixel serial port %d in jhcEliBody::Reset !\n", dport);
    }
    dyn.Reset();

    // tell other components to reset individually
    arm.Reset(rpt, 1);
    neck.Reset(rpt, 1);
    base.Reset(rpt, 1);
    lift.Reset(rpt, 1);
    mic.mport = 8;                 // serial port for sound direction
    mic.Reset(rpt);
  }

  // finished with actuators
  if (rpt > 0)
  {
    jprintf("\n");
    jprintf("BODY -> %s\n", ((CommOK(0) > 0) ? "OK" : "FAILED !!!"));
    jprintf("=========================\n");
    jprintf("\n");
  }

  // zero idle counts
  ntime = neg5;
  ltime = neg5;
  atime = neg5;
  gtime = neg5;
  ttime = neg5;
  mtime = neg5;

  // clear performance timer and report overall status
  for (i = 0; i < 10; i++)
    tcmd[i] = 0;
  tfill = 0;
//  dyn.Reset();
  chk_vid(1);
  return CommOK();
}


//= Make sure receiving images are correct size.

void jhcEliBody::chk_vid (int start)
{
  // defaults
  iw = 640;
  ih = 480;
  cw = 640;
  ch = 480;
  tstep = 33;

  // set proper color image size
  if (vid == NULL)
    return;
  cw = vid->XDim();
  ch = vid->YDim();

  // set frame rate
  tstep = vid->StepTime();
  if (vid->Dual() > 0)
    tstep = vid->StepTime(1);

  // make up receiving images
  vid->SizeFor(col, 0);
  vid->SizeFor(rng, 1);
  if (cw > iw)
    col2.SetSize(iw, ih, 3);

  // possible start source
  if (start > 0)
    vid->Rewind(1);
}


//= Get likely configuration file name based on robot number.
// useful for other body-based parameters other than actuators (e.g. speech)
// can optionally talk to PIC controller if needed to get ID
// returns 1 if likely not read yet, 0 if done previously, -1 if not found

int jhcEliBody::CfgFile (char *fname, int chk, int ssz)
{
  FILE *in;
  int first = ((bnum < 0) ? 1 : 0);

  if ((bnum <= 0) && (chk > 0))
  {
    // connect to proper serial port (if needed) 
    if (mok < 0)
      if (dyn.SetSource(dport, dbaud, 256) > 0)
        mok = 1;

    // ask Dynamixel PIC controller for body serial number
    dyn.Reset();
    bnum = dyn.RobotID();
  }

  // look in local directory
  sprintf_s(fname, ssz, "robot-%d.cfg", __max(0, bnum));
  if (fopen_s(&in, fname, "r") != 0)
  {
    // look in parallel directory
    sprintf_s(fname, ssz, "../config/robot-%d.cfg", __max(0, bnum));
    if (fopen_s(&in, fname, "r") != 0)
    {
      *fname = '\0';
      return -1;
    }
  }

  // cleanup
  fclose(in);
  return first;
}


//= Tell if all communications seem to be working properly.
// generally -1 = not open, 0 = protocol error, 1 = okay

int jhcEliBody::CommOK (int rpt, int bad) const
{
  int ok = mok;

  // compute overall value as OR of components
  ok = __min(ok, arm.CommOK(bad));
  ok = __min(ok, neck.CommOK(bad));
  ok = __min(ok, base.CommOK(bad));
  ok = __min(ok, lift.CommOK(bad));
  ok = __min(ok, mic.CommOK(bad));

  // tell reason for failure (if any)
  if ((ok <= 0) && (rpt > 0))
    jprintf("!!! Comm failure:%s%s%s%s%s !!!\n\n",
            ((arm.CommOK(bad)  <= 0) ? " arm" : ""), 
            ((neck.CommOK(bad) <= 0) ? " neck" : ""), 
            ((base.CommOK(bad) <= 0) ? " base" : ""), 
            ((lift.CommOK(bad) <= 0) ? " lift" : ""), 
            ((mic.CommOK(bad)  <= 0) ? " mic" : ""));
  return ok;
}


//= Generate a string suitable for TTS listing all hardware problems.
// returns NULL if no problems 

const char *jhcEliBody::Problems ()
{
  char sys[5][40] = {"arm", "neck", "wheels", "lift stage", "direction sensor"}; 
  int bad[5] = {0, 0, 0, 0, 0};
  int i, n = 0, last = -1;

  // catalog bad systems
  if (arm.CommOK() <= 0)
    bad[0] = 1;
  if (neck.CommOK() <= 0)
    bad[1] = 1;
  if (base.CommOK() <= 0)
    bad[2] = 1;
  if (lift.CommOK() <= 0)
    bad[3] = 1;
  if (mic.CommOK() <= 0)
    bad[4] = 1;

  // count problems and find last one (if any)
  for (i = 0; i < 5; i++)
    if (bad[i] > 0)
    {
      last = i;
      n++;
    }
  if (n <= 0)
    return NULL;    

  // build output string
  *errors = '\0';
  for (i = 0; i <= last; i++)
    if (bad[i] > 0)
    {
      if (*errors != '\0')
        strcat_s(errors, ", ");
      if ((n > 1) && (i == last))
        strcat_s(errors, "and ");
      strcat_s(errors, sys[i]);
    }
  return errors;
}  


//= Tell what percentage of mega-update packets failed.

double jhcEliBody::MegaReport ()
{
  double pct;

  if (dyn.mpod <= 0)
    return 0.0;
  pct = (100.0 * dyn.mfail) / dyn.mpod;
  jprintf("  Dynamixel %4.2f pct failed (%d out of %d)\n", 
          pct, dyn.mfail, dyn.mpod);
  return pct;
}


//= Tell how many seconds since some body actuator had a high bid command.
// needs to be supplied with current time (e.g. jms_now())

double jhcEliBody::BodyIdle (UL32 now) const
{
  int dt, ms = jms_diff(now, ntime);

  if ((dt = jms_diff(now, ltime)) < ms)
    ms = dt;
  if ((dt = jms_diff(now, atime)) < ms)
    ms = dt;
  if ((dt = jms_diff(now, gtime)) < ms)
    ms = dt;
  if ((dt = jms_diff(now, ttime)) < ms)
    ms = dt;
  if ((dt = jms_diff(now, mtime)) < ms)
    ms = dt;
  return(0.001 * ms);
}


///////////////////////////////////////////////////////////////////////////
//                           Kinect Image Access                         //
///////////////////////////////////////////////////////////////////////////

//= Get color image that matches the size of the depth image (640 480).
// will downsample if hi-res Kinect mode selected

int jhcEliBody::ImgSmall (jhcImg& dest) 
{
  if (!dest.SameFormat(col))
    return Smooth(dest, col);
  return dest.CopyArr(col);
}


//= Get color image in the highest resolution available.
// will upsample if low-res Kinect mode selected

int jhcEliBody::ImgBig (jhcImg& dest) 
{
  if (!dest.SameFormat(col))
    return Bicubic(dest, col);
  return dest.CopyArr(col); 
} 


//= Get depth image as an 8 bit gray scale rendering.
// generally better for display purposes

int jhcEliBody::Depth8 (jhcImg& dest) const 
{
  if (!rng.Valid())
    return dest.FillArr(0);
  if (!dest.Valid(2))
    return Night8(dest, rng, vid->Shift);
  return dest.CopyArr(rng);
}


//= Get depth image with full 16 bit resolution.
// upconverts if 8 bit gray scale version (e.g. from file)

int jhcEliBody::Depth16 (jhcImg& dest) const 
{
  if (!rng.Valid())
    return dest.FillArr(0);
  if (!dest.Valid(1))
    return Fog16(dest, rng);  
  return dest.CopyArr(rng);
}


///////////////////////////////////////////////////////////////////////////
//                              Basic Actions                            //
///////////////////////////////////////////////////////////////////////////

//= Stop all motion and hold current position.
// generally should call Update just before this

int jhcEliBody::Freeze ()
{
  lift.Freeze();
  base.Freeze();
  arm.Freeze();
  neck.Freeze();
  return CommOK();
}


//= Stop all motion and go passive (where possible).

int jhcEliBody::Limp ()
{
  lift.Limp();
  base.Limp();
  arm.Limp();
  neck.Limp();
  return CommOK();
}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Load new images from video source (e.g. Kinect).
// Note: BLOCKS until frame(s) become available

int jhcEliBody::UpdateImgs ()
{
  if (vid == NULL)
    return -1;
  if (vid->Dual() > 0)
    return vid->DualGet(col, rng);
  return vid->Get(col);              // sometimes useful (e.g. face enroll)
}


//= Load in fresh configuration data from all mechanical elements.
// NOTE: if voice < 0 then mic.Update should be called separately
// useful since mic depends on voice while nothing else does

int jhcEliBody::Update (int voice, int imgs, int bad)
{
  // possibly skip getting new images (for timing usually)
  if (imgs > 0)
    if (UpdateImgs() <= 0)
      return 0;

  // possibly determine sound directions, request new servo data
  if (voice >= 0)
    mic.Update(voice);
  if (mega > 0)
    dyn.MegaIssue(id0, idn);

  // request first base value and lift data 
  base.UpdateStart();
  lift.UpdateStart();

  // collect first base value and lift data, request second base value 
  base.UpdateContinue();
  lift.UpdateFinish();

  // read AX-12 servo data, possibly from earlier request
  if (mega > 0)
    dyn.MegaCollect();
  neck.Update();
  arm.Update(0);             // mega already called if applicable

  // collect second base value
  base.UpdateFinish();
  return CommOK(1, bad);
}


//= Tell neck angles and true height of camera above floor.

void jhcEliBody::CamPose (double& pan, double& tilt, double& ht)
{
  jhcMatrix pos(4);

  pan = neck.Pan();
  tilt = neck.Tilt();
  neck.HeadLoc(pos, lift.Height());
  ht = pos.Z();
}


//= Have all mechanical elements move now that command arbitration is done.

int jhcEliBody::Issue (double lead)
{
  double tvid = 0.001 * tstep, tupd = tvid, diff = 0.0;
  int n, t, t0 = tfill, tspan = 3;
  UL32 tnow = jms_now();

  // store current time in circular buffer
  tcmd[tfill] = tnow;
  if (++tfill >= 10)
    tfill = 0;

  // get average over last few steps
  for (n = 0; n < tspan; n++)
  {
    // find difference in seconds for valid readings
    t = t0;
    if (--t0 < 0)           
      t0 += 10;
    if (tcmd[t0] == 0)
      break;
    diff += 0.001 * (tcmd[t] - tcmd[t0]);
  }

  // clamp to sensible limits
  if (n > 0)
  {
    diff /= (double) n;
    tupd = __max(tvid, __min(diff, 0.5));
  }

  // tell components to issue their commands 
  arm.Issue(tupd, lead, 0);  
  neck.Issue(tupd, lead);          // send arm & neck servos 
  base.Issue(tupd, lead);
  lift.Issue(tupd, lead);

  // update last high bid time
  if (neck.GazeWin() >= nbid)
    ntime = tnow;
  if (lift.LiftWin() >= lbid)
    ltime = tnow;
  if (arm.ArmWin() >= abid)
    atime = tnow;
  if (arm.HandWin() >= gbid)
    gtime = tnow;
  if (base.TurnWin() >= tbid)
    ttime = tnow;
  if (base.MoveWin() >= mbid)
    mtime = tnow;
  return CommOK();
}


///////////////////////////////////////////////////////////////////////////
//                          Ballistic Functions                          //
///////////////////////////////////////////////////////////////////////////

//= Make the robot beep (blocks).

void jhcEliBody::Beep ()
{
  ::Beep(750, 300);
}


//= Assume the standard ready pose and optionally set arm height (blocks).
// retract arm and close hand, set lift to desired height, look straight ahead 

int jhcEliBody::InitPose (double ht)
{
  int ok = 1;

  if (arm.ZeroGrip(1) <= 0)
    ok = -3;
  if (arm.Stow() <= 0)
    ok = -2;
  if (ht >= 0.0)                                           // skip if negative
    if (lift.SetLift((ht > 0.0) ? ht : lift.ht0) <= 0)
      ok = -1;
  if (neck.SetNeck(0.0, neck.gaze0) <= 0)
    ok = 0;
  return ok;
} 