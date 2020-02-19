// jhcSocial.cpp : interface to ELI people tracking kernel for ALIA system
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 IBM Corporation
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

#include <math.h>
#include <string.h>

#include "Interface/jms_x.h"           // common video

#include "Grounding/jhcSocial.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcSocial::~jhcSocial ()
{
}


//= Default constructor initializes certain values.

jhcSocial::jhcSocial ()
{
  ver = 1.00;
  strcpy_s(tag, "Social");
  Platform(NULL);
  rpt = NULL;
  dbg = 0;
  Defaults();
}


//= Attach physical enhanced body and make pointers to some pieces.

void jhcSocial::Platform (jhcEliGrok *robot) 
{
  rwi = robot;
}


///////////////////////////////////////////////////////////////////////////
//                         Processing Parameters                         //
///////////////////////////////////////////////////////////////////////////

//= Parameters used for detecting close people.

int jhcSocial::evt_params (const char *fname)
{
  jhcParam *ps = &eps;
  int ok;

  ps->SetTag("soc_evt", 0);
  ps->NextSpecF( &pnear, 40.0, "Person near alert (in)");
  ps->NextSpecF( &pfar,  48.0, "Person far forget (in)");
  ok = ps->LoadDefs(fname);
  ps->RevertAll();
  return ok;
}


///////////////////////////////////////////////////////////////////////////
//                           Parameter Bundles                           //
///////////////////////////////////////////////////////////////////////////

//= Read all relevant defaults variable values from a file.

int jhcSocial::Defaults (const char *fname)
{
  int ok = 1;

  ok &= evt_params(fname);
  return ok;
}


//= Write current processing variable values to a file.

int jhcSocial::SaveVals (const char *fname) const
{
  int ok = 1;

  ok &= eps.SaveVals(fname);
  return ok;
}


///////////////////////////////////////////////////////////////////////////
//                          Overridden Functions                         //
///////////////////////////////////////////////////////////////////////////

//= Set up for new run of system.

void jhcSocial::local_reset (jhcAliaNote *top)
{
  rpt = top;
  hwin = -1;
}


//= Post any spontaneous observations to attention queue.

void jhcSocial::local_volunteer ()
{
  see_vip();   
  person_close(); 
}


//= Start up a new instance of some named function.
// starting time and bid are already speculatively bound by base class
// variables "desc" and "i" must be bound for macro dispatcher to run properly
// returns 1 if successful, -1 for problem, -2 if function unknown

int jhcSocial::local_start (const jhcAliaDesc *desc, int i)
{
//  JCMD_SET(ball_stop);
  return -2;
}


//= Check on the status of some named function.
// variables "desc" and "i" must be bound for macro dispatcher to run properly
// returns 1 if done, 0 if still working, -1 if failed, -2 if function unknown

int jhcSocial::local_status (const jhcAliaDesc *desc, int i)
{
//  JCMD_CHK(ball_stop);
  return -2;
}


///////////////////////////////////////////////////////////////////////////
//                           Reported Events                             //
///////////////////////////////////////////////////////////////////////////

//= Inject NOTE saying a particular person's face has just been recognized.
// states: "I see X"

void jhcSocial::see_vip ()
{
  jhcBodyData *p;
  jhcAliaDesc *n, *act;
  int i;

  // lock to sensor cycle 
  if ((rpt == NULL) || (rwi == NULL) || (rwi->body == NULL))
    return;
  if (!rwi->Accepting())
    return;

  // see if new person just recognized (make semantic node if needed) 
  if ((i = (rwi->fn).JustNamed()) < 0)
    return;
  p = (rwi->s3).RefPerson(i);
  if ((n = (jhcAliaDesc *) p->node) == NULL)
  {
    n = rpt->NewNode("agt", NULL, 0, -1.0);
    p->node = (void *) n;
  }

  // generate message to core reasoner
  rpt->StartNote();
  act = rpt->NewNode("act", "see");
  rpt->AddArg(act, "agt", rpt->Self());
  rpt->AddArg(act, "obj", n);
  add_name(n, (rwi->fn).FaceName(i));
  rpt->FinishNote();
}


//= Generate an event whenever a person gets inside the robot's personal space.
// states: "X is close" where X is a person and perhaps has a name

void jhcSocial::person_close ()
{
  jhcBodyData *p;
  jhcAliaDesc *n;
  double d;
  int old = hwin;

  // lock to sensor cycle 
  if ((rpt == NULL) || (rwi == NULL) || (rwi->body == NULL))
    return;
  if (!rwi->Accepting())
    return;

  // see if a person newly close (use hysteresis)
  if ((hwin = (rwi->s3).Closest()) < 0)
    return;
  p = (rwi->s3).RefPerson(hwin);
  d = p->PlaneVec3();
  if ((d > pnear) && ((hwin != old) || (d > pfar)))
    hwin = -1;        
  if ((hwin < 0) || (old >= 0))                 
    return;

  // make semantic node if needed
  if ((n = (jhcAliaDesc *) p->node) == NULL)
  {
    n = rpt->NewNode("agt", NULL, 0, -1.0);
    p->node = (void *) n;
  }

  // generate message to core reasoner
  rpt->StartNote();
  rpt->NewProp(n, "hq", "close");
  add_name(n, (rwi->fn).FaceName(hwin));
  rpt->FinishNote();
}


//= Add both parts of face recognition name to some user node.
// takes index of person (not ID)

void jhcSocial::add_name (jhcAliaDesc *n, const char *name) const
{
  char first[80];
  const jhcAliaDesc *kind;
  char *end;
  int i = 0;

  // add personhood if missing
  if (n == NULL) 
    return;
  while ((kind = n->Fact("ako", i++)) != NULL)
    if (kind->HasWord("person"))
      break;
  if (kind == NULL)
    rpt->NewProp(n, "ako", "person");

  // possibly add full name ("Jon Connell")
  if ((name == NULL) || (*name == '\0'))
    return;
  if (!n->HasWord(name))
    rpt->NewLex(n, name);

  // possibly add first name ("Jon")
  strcpy_s(first, name);
  if ((end = strchr(first, ' ')) != NULL)
    *end = '\0';
  if (!n->HasWord(first))
    rpt->NewLex(n, first);
}


/*
///////////////////////////////////////////////////////////////////////////
//                             Overall Poses                             //
///////////////////////////////////////////////////////////////////////////

//= Start freeze of translation and rotation.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_stop_set (const jhcAliaDesc *desc, int i)
{
  ct0[i] += ROUND(1000.0 * ftime);
  return 1;
}


//= Continue freeze of translation and rotation until timeout.
// sets up continuing request to body if not finished
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_stop_chk (const jhcAliaDesc *desc, int i)
{
  jhcMatrix pos(4), dir(4);
  jhcEliBase *b;
  jhcEliArm *a;

  // lock to sensor cycle 
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  if (!rwi->Accepting())
    return 0;
  b = rwi->base;
  a = rwi->arm;

  // check for timeout
  if (jms_diff(jms_now(), ct0[i]) > 0)
    return 1;
  a->ArmPose(pos, dir);
 
  // re-issue basic command (coast to stop, no bouncing)
  jprintf(1, dbg, ">> REQUEST %d: stop motion\n", cbid[i]);
  a->ArmTarget(pos, dir, 1.0, 1.0, cbid[i]);
  b->DriveTarget(0.0, 0.0, 1.0, cbid[i]);
  return 0;
}


///////////////////////////////////////////////////////////////////////////
//                              Translation                              //
///////////////////////////////////////////////////////////////////////////

//= Convert semantic network into a nuanced move command and request it.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_drive_set (const jhcAliaDesc *desc, int i)
{
  if (get_vel(csp[i], desc->Val("arg")) <= 0)
    return -1;
  if (get_dist(camt[i], desc->Val("arg")) <= 0)
    return -1;
  ct0[i] = 0;
  return 1;
} 


//= Check whether move command is done yet.
// sets up continuing request to body if not finished
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_drive_chk (const jhcAliaDesc *desc, int i)
{
  jhcEliBase *b;
  double err;

  // lock to sensor cycle
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  if (!rwi->Accepting())
    return 0;
  b = rwi->base;

  if (cst[i] <= 0)
  {
    // set up absolute target distance
    camt[i] = b->MoveGoal(camt[i]);
    cerr[i] = b->MoveErr(camt[i]);
    cst[i]  = 1;
  }
  else 
  {
    // check if finished or stuck
    err = b->MoveErr(camt[i]);
    jprintf(2, dbg, "move: %3.1f, err = %3.1f, stuck = %d\n", b->Travel(), err, ct0[i]);
    if (err < (1.5 * b->mdead))
      return 1;
    if (Stuck(i, err, mprog, mstart, mmid))
      return -1;
  }

  // re-issue basic command (move and turn are separate resources)
  jprintf(1, dbg, ">> REQUEST %d: move @ %3.1f in\n", cbid[i], camt[i]);
  b->MoveAbsolute(camt[i], csp[i], cbid[i]);
  return 0;
}


//= Read semantic network parts to determine amount of travel.
// step = 4", move = 8", drive = 16"
// returns 1 if proper values, -1 for problem

int jhcSocial::get_dist (double& dist, const jhcAliaDesc *act) const
{
  const jhcAliaDesc *dir;

  // sanity check
  if (act == NULL)
    return -1;

  // set distance based on main verb
  if (act->WordIn("step"))
    dist = step;
  else if (act->WordIn("move"))
    dist = move;
  else if (act->WordIn("drive"))
    dist = drive;
  else
    return -1;

  // get directional modifier of main verb 
  if ((dir = act->Fact("dir")) != NULL)
  {
    // see if some standard direction term (checks halo also)
    if (dir->WordIn("backward", "backwards"))
      dist = -dist;
    else if (!dir->WordIn("forward", "forwards"))
      return -1;
  }
  return 1;
}


//= Read semantic network parts to determine speed of travel.
// <pre>
//
// "drive backward"
//   act-1 -lex-  drive
//   dir-2 -lex-  backward
//         -dir-> act-1
//   mod-3 -lex-  slowly
//         -mod-> act-1
//
// </pre>
// returns 1 if proper values, -1 for problem

int jhcSocial::get_vel (double& speed, const jhcAliaDesc *act) const
{
  const jhcAliaDesc *rate;
  int w = 0;

  // sanity check
  if (act == NULL)
    return -1;

  // look for speed modifier(s)
  speed = 1.0;
  while ((rate = act->Fact("mod", w++)) != NULL)
  {
    if (rate->WordIn("slowly"))
      speed *= stf;
    else if (rate->WordIn("quickly"))
      speed *= qtf;
  }
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                                Rotation                               //
///////////////////////////////////////////////////////////////////////////

//= Convert semantic network into a nuanced turn command and request it.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_turn_set (const jhcAliaDesc *desc, int i)
{
  if (get_spin(csp[i], desc->Val("arg")) <= 0)
    return -1;
  if (get_ang(camt[i], desc->Val("arg")) <= 0)
    return -1;
  ct0[i] = 0;
  return 1;
} 


//= Check whether turn command is done yet.
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_turn_chk (const jhcAliaDesc *desc, int i)
{
  jhcEliBase *b;
  double err;

  // lock to sensor cycle
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  if (!rwi->Accepting())
    return 0;
  b = rwi->base;

  if (cst[i] <= 0)
  {
    // set up absolute target angle
    camt[i] = b->TurnGoal(camt[i]);
    cerr[i] = b->TurnErr(camt[i]);
    cst[i]  = 1;
  }
  else 
  {
    // check if finished or stuck
    err = b->TurnErr(camt[i]);
    jprintf(2, dbg, "turn: %3.1f, err = %4.2f, stuck = %d\n", b->WindUp(), err, ct0[i]);
    if (err < (1.5 * b->tdead))
      return 1;
    if (Stuck(i, err, tprog, tstart, tmid))
      return -1;
  }

  // re-issue basic command (move and turn are separate resources)
  jprintf(1, dbg, ">> REQUEST %d: turn @ %3.1f deg\n\n", cbid[i], camt[i]);
  b->TurnAbsolute(camt[i], csp[i], cbid[i]);
  return 0;
}


//= Read semantic network to get amount to rotate.
// turn = 90 degs, rotate = 180 degs, spin = 360 degs
// returns 1 if proper values, -1 for problem

int jhcSocial::get_ang (double& ang, const jhcAliaDesc *act) const
{
  const jhcAliaDesc *dir;

  // sanity check
  if (act == NULL)
    return -1;
  ang = turn;

  // get angle based on main verb
  if (act->WordIn("spin"))
    ang = spin;
  else if (act->WordIn("rotate"))
    ang = rot;
  else if (!act->WordIn("turn"))
    return -1;

  // get directional modifier of main verb 
  if ((dir = act->Fact("dir")) != NULL)
  {
    // see if some standard direction term (checks halo also)
    if (dir->WordIn("clockwise", "right"))
      ang = -ang;
    else if (!dir->WordIn("counterclockwise", "left"))
      return -1;
  }
  return 1;
}


//= Read semantic network parts to determine direction to turn.
// <pre>
//
// "turn clockwise"
//   act-1 -lex-  turn
//   dir-2 -lex-  clockwise
//         -dir-> act-1         
//   mod-3 -lex-  quickly
//         -mod-> act-1
//
// </pre>
// returns 1 if proper values, -1 for problem

int jhcSocial::get_spin (double& speed, const jhcAliaDesc *act) const
{
  const jhcAliaDesc *rate;
  int w = 0;

  // sanity check
  if (act == NULL)
    return -1;

  // look for speed modifier(s)
  speed = 1.0;
  while ((rate = act->Fact("mod", w++)) != NULL)
  {
    if (rate->WordIn("slowly"))
      speed *= srf;                         
    else if (rate->WordIn("quickly"))
      speed *= qrf;
  }
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                                  Lift                                 //
///////////////////////////////////////////////////////////////////////////

//= Convert semantic network into a nuanced lift command and request it.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_lift_set (const jhcAliaDesc *desc, int i)
{
  if (get_up(camt[i], desc->Val("arg")) <= 0)
    return -1;
  if (get_vsp(csp[i], desc->Val("arg")) <= 0)
    return -1;
  ct0[i] = 0;  
  return 1;
} 


//= Check whether lift command is done yet.
// sets up continuing request to body if not finished
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_lift_chk (const jhcAliaDesc *desc, int i)
{
  jhcEliLift *f;
  double err;

  // lock to sensor cycle
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  if (!rwi->Accepting())
    return 0;
  f = rwi->lift;

  if (cst[i] <= 0)
  {
    // set up absolute target angle
    camt[i] = f->LiftGoal(camt[i]);
    cerr[i] = f->LiftErr(camt[i]);
    cst[i]  = 1;
  }
  else 
  {
    // check if finished or stuck
    err = f->LiftErr(camt[i]);
    jprintf(2, dbg, "lift: %3.1f, err = %3.1f, stuck = %d\n", f->Height(), err, ct0[i]);
    if (err < f->ldone)
      return 1;
    if (Stuck(i, err, lprog, lstart, lmid))
      return -1;
  }

  // re-issue basic command
  jprintf(1, dbg, ">> REQUEST %d: lift @ %3.1f in\n\n", cbid[i], camt[i]);
  f->LiftTarget(camt[i], csp[i], cbid[i]);
  return 0;
}


//= Read semantic network parts to determine direction to move lift stage
// returns 1 if proper values, -1 for problem

int jhcSocial::get_up (double& dist, const jhcAliaDesc *act) const
{
  jhcAliaDesc *amt;

  // sanity check
  if (act == NULL)
    return -1;
  dist = lift;

  // possibly go to some extreme
  if ((amt = act->Fact("amt")) != NULL)
    if (amt->WordIn("all the way"))
       dist = 50.0;

  // get direction based on verb
  if (act->WordIn("lower"))
    dist = -dist;
  else if (!act->WordIn("raise"))
    return -1;
  return 1;
}


//= Read semantic network parts to determine speed for lift.
// <pre>
//
// "raise the arm"
//   act-1 -lex-  raise
//         -obj-> obj-3
//   ako-4 -lex-  arm
//         -ako-> obj-3
//   mod-3 -lex-  slowly
//         -mod-> act-1
//
// </pre>
// returns 1 if proper values, -1 for problem

int jhcSocial::get_vsp (double& speed, const jhcAliaDesc *act) const
{
  const jhcAliaDesc *rate;
  int w = 0;

  // sanity check
  if (act == NULL)
    return -1;

  // look for speed modifier(s)
  speed = 1.0;
  while ((rate = act->Fact("mod", w++)) != NULL)
  {
    if (rate->WordIn("slowly"))
      speed *= slf;            
    else if (rate->WordIn("quickly"))
      speed *= qlf;
  }
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                                Gripper                                //
///////////////////////////////////////////////////////////////////////////

//= Convert semantic network into a nuanced grip command and request it.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_grip_set (const jhcAliaDesc *desc, int i)
{
  if (get_hand(camt[i], desc->Val("arg")) <= 0)
    return -1;
  ct0[i] = 0;  
  return 1;
}


//= Check whether grip command is done yet.
//    hold state (csp > 0): 0 save pose, 1 width mode start, 2 width mode mid, 3 force mode
// release state (csp < 0): 0 save pose, 1 width mode start, 2 width mode mid
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_grip_chk (const jhcAliaDesc *desc, int i)
{
  jhcEliArm *a;
  const char *act = ((camt[i] < 0.0) ? "hold" : ((camt[i] > 2.0) ? "open" : "close"));
  double err, stop = __max(0.0, camt[i]);

  // lock to sensor cycle
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  if (!rwi->Accepting())
    return 0;
  a = rwi->arm;

  if (cst[i] <= 0)
  {
    // remember initial finger center position
    a->ArmPose(cpos[i], cdir[i]);
    cerr[i] = a->WidthErr(camt[i]);
    cst[i]  = 1;
  }
  else if (cst[i] <= 2)
  {
    // check if target width achieved or stuck
    err = a->WidthErr(stop);
    jprintf(2, dbg, "%s[%d]: width = %3.1f in, force = %3.1f, stuck = %d\n", 
            act, cst[i], a->Width(), a->Squeeze(), ct0[i]);
    if (err < wtol)
      return((camt[i] < 0.0) ? -1 : 1);                // full close = hold fail
    if ((camt[i] < 0.0) && a->SqueezeSome(fhold))
    {
      // if holding, switch to force mode after initial contact 
      ct0[i] = 0;  
      cst[i] = 3;                                  
    }
    if (Stuck(i, err, gprog, gstart, gmid))
      return -1;
  }
  else if (cst[i] >= 3)
  {
    // request force application for a while (always succeeds)
    err = a->SqueezeErr(fhold, 0);
    jprintf(2, dbg, "hold[3]: width = %3.1f, force = %3.1f, good = %d, try = %d\n", 
            a->Width(), a->Squeeze(), ROUND(csp[i]), ct0[i]);
    if (ct0[i]++ >= (UL32) fask)  
      return 1;
  }
          
  // re-issue basic width or force command (keep finger center in same place)
  a->ArmTarget(cpos[i], cdir[i], 1.0, 1.0, cbid[i]);
  if (cst[i] <= 2)
  {
    jprintf(1, dbg, ">> REQUEST %d: %s @ %3.1f in\n\n", cbid[i], act, camt[i]);
    a->WidthTarget(camt[i], 1.0, cbid[i]);
  }
  else if (cst[i] >= 3)
  {
    jprintf(1, dbg, ">> REQUEST %d: hold @ %3.1f oz force\n\n", cbid[i], fhold);
    a->SqueezeTarget(fhold, cbid[i]);
  }
  return 0;
}


//= Read semantic network parts to determine whether to open or close.
// <pre>
//
// "hold this"
//   act-1 -lex-  hold
//         -obj-> obj-3
//
// </pre>
// width = -0.5 for "hold" (force), 0.1 for "close" (width), 3.3 for "open" (width) 
// returns 1 if proper values, -1 for problem

int jhcSocial::get_hand (double &width, const jhcAliaDesc *act) const
{
  // sanity check
  if ((act == NULL) || (rwi == NULL) || (rwi->body == NULL))
    return -1;
  width = 0.1;

  // get hold status based on main verb
  if (act->WordIn("open", "release"))
    width = (rwi->arm)->MaxWidth();
  else if (act->WordIn("hold"))
    width = -0.5;
  else if (!act->WordIn("close"))
    return -1;
  return 1;
}


///////////////////////////////////////////////////////////////////////////
//                                Arm                                    //
///////////////////////////////////////////////////////////////////////////

//= Convert semantic network into a nuanced arm command and request it.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_arm_set (const jhcAliaDesc *desc, int i)
{
  if ((cst[i] = get_pos(i, desc->Val("arg"))) < 0)
    return -1;
  cerr[i] = cpos[i].LenVec3();       // not accurate for absolute position
  ct0[i] = 0;  
  return 1;
} 


//= Check whether arm command is done yet.
// sets up continuing request to body if not finished
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_arm_chk (const jhcAliaDesc *desc, int i)
{
  jhcMatrix now(4);
  char txt[40];
  jhcEliArm *a;
  double err, zerr;

  // lock to sensor cycle
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  if (!rwi->Accepting())
    return 0;
  a = rwi->arm;

  if (cst[i] <= 0)
  {
    // set up absolute position based on hand direction (pan tilt roll)
    a->ArmPose(now, cdir[i]);
    cpos[i].RotPan3(cdir[i].P());
    cpos[i].IncVec3(now);
    cst[i] = 1;
  }
  else
  {
    // check if finished or stuck
    a->Position(now);
    err = now.PosDiff3(cpos[i]);
    zerr = a->ErrZ(cpos[i]);
    if (cdir[i].W() < 0.0)
      err = __max(err, a->Width());
    jprintf(2, dbg, "hand: %s, err = %3.1f in (%3.1f), stuck = %d\n", now.ListVec3(txt), err, zerr, ct0[i]);
    if ((err < hdone) && (zerr < zdone))
      return 1;
    if (Stuck(i, err, hprog, hstart, hmid))
      return -1;
  }

  // re-issue basic command (arm and wrist are combined, hand separate)
  jprintf(1, dbg, ">> REQUEST %d: hand @ %s\n\n", cbid[i], cpos[i].ListVec3(txt));
  a->ArmTarget(cpos[i], cdir[i], 1.0, 1.0, cbid[i]);
  if (cdir[i].W() < 0.0)
    a->WidthTarget(0.0);
  return 0;
}


//= Read semantic network parts to determine desired new hand postition.
// returns 1 if absolute, 0 if relative to current, -1 for problem

int jhcSocial::get_pos (int i, const jhcAliaDesc *act) 
{
  jhcEliArm *a;
  jhcAliaDesc *dir;
  int w = 0;

  // sanity check
  if ((act == NULL) || (rwi == NULL) || (rwi->body == NULL))
    return -1;
  a = rwi->arm;

  // absolute position based on main verb
  if (act->WordIn("retract"))
  {
    cpos[i].SetVec3(a->retx, a->rety, a->retz);        
    cdir[i].SetVec3(a->rdir, a->rtip, 0.0, -1.0);    // forced closed
    return 1;
  }
  if (act->WordIn("extend"))
  {
    cpos[i].SetVec3(extx, exty, extz);  
    cdir[i].SetVec3(edir, etip, 0.0, 0.0);           // width unspecified
    return 1;
  }

  // find direction based on modifier(s)
  cpos[i].Zero(1.0);
  while ((dir = act->Fact("dir", w++)) != NULL)
  {
    // get pointing offset (assume hand is along x axis)
    if (dir->WordIn("forward", "forwards"))
      cpos[i].SetX(dxy);
    else if (dir->WordIn("backward", "backwards"))
      cpos[i].SetX(-dxy);

    // get lateral offset
    if (dir->WordIn("left"))
      cpos[i].SetY(dxy);
    else if (dir->WordIn("right"))
      cpos[i].SetY(-dxy);

    // get vertical offset
    if (dir->WordIn("up"))
      cpos[i].SetZ(dz);
    else if (dir->WordIn("down"))
      cpos[i].SetZ(-dz);
  }

  // make sure some valid direction was specified (e.g. not CCW)
  if (cpos[i].LenVec3() == 0.0)
    return -1;
  return 0;
}


///////////////////////////////////////////////////////////////////////////
//                               Wrist                                   //
///////////////////////////////////////////////////////////////////////////

//= Convert semantic network into a nuanced wrist command and request it.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_wrist_set (const jhcAliaDesc *desc, int i)
{
  if ((cst[i] = get_dir(i,  desc->Val("arg"))) < 0)
    return -1;
  cerr[i] = cdir[i].MaxAbs3();
  ct0[i] = 0;
  return 1;
} 


//= Check whether wrist command is done yet.
// sets up continuing request to body if not finished
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_wrist_chk (const jhcAliaDesc *desc, int i)
{
  jhcEliArm *a;
  jhcMatrix now(4);
  char txt[40];
  double err;

  // lock to sensor cycle
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  if (!rwi->Accepting())
    return 0;
  a = rwi->arm;

  if (cst[i] <= 0)
  {
    // set up absolute orientation based on current hand direction (pan tilt roll)
    a->ArmPose(cpos[i], now);
    cdir[i].IncVec3(now);
    cdir[i].CycNorm3();
    cst[i] = 2;
  }
  else if (cst[i] == 1)
  {
    // change zero components to current angles
    a->ArmPose(cpos[i], now);
    cdir[i].SubZero3(now);
    cst[i] = 2;
  }
  else
  {
    // check if finished or stuck
    a->Direction(now);
    err = now.RotDiff3(cdir[i]);
    jprintf(2, dbg, "wrist: %s, err = %3.1f deg, stuck = %d\n", now.ListVec3(txt), err, ct0[i]);
    if (err < wdone)
      return 1;
    if (Stuck(i, err, wprog, wstart, wmid))
      return -1;
  }

  // re-issue basic command (arm and wrist are combined, hand separate)
  jprintf(1, dbg, ">> REQUEST %d: wrist @ %s\n\n", cbid[i], cdir[i].ListVec3(txt));
  a->ArmTarget(cpos[i], cdir[i], 1.0, 1.0, cbid[i]);
  return 0;
}


//= Read semantic network parts to determine desired new hand orientation.
// cdir[i] hold pan and tilt changes, assumes hand is pointing along x
// returns 1 if partial absolute, 0 if relative to current, -1 for problem

int jhcSocial::get_dir (int i, const jhcAliaDesc *act) 
{
  jhcAliaDesc *dir;
  int w = 0;

  // sanity check
  if (act == NULL)
    return -1;
  cdir[i].Zero();

  // absolute position based on main verb 
  if (act->WordIn("reset"))
  {
    cdir[i].SetT(etip);         
    return 1;                // partial absolute
  }

  // possibly roll some specified direction ("twist")
  if (act->WordIn("twist"))
  {
    if ((dir = act->Fact("dir")) == NULL)
      return -1;
    if (dir->WordIn("counterclockwise", "leftt"))
      cdir[i].SetR(-wroll);
    else if (dir->WordIn("clockwise", "right"))
      cdir[i].SetR(wroll);
    else
      return -1;
    return 0;                // relative
  }

  // possibly get absolute pose for "point"
  if ((dir = act->Fact("dir")) == NULL)
    return -1;
  if (dir->WordIn("vertical"))
  {
    cdir[i].SetT(-90.0);
    return 1;
  }
  if (dir->WordIn("horizontal"))
  {
    // can combine with in-plane angle
    cdir[i].SetT(-0.1);
    if (dir->WordIn("forward", "forwards"))
      cdir[i].SetP(90.0);
    else if (dir->WordIn("sideways"))
      cdir[i].SetP(180.0);
    return 1;                // partial absolute (hence -0.1)
  }

  // find direction based on modifier(s)
  while ((dir = act->Fact("dir", w++)) != NULL)
  {
    // get incremental pan offset
    if (dir->WordIn("left"))
      cdir[i].SetP(wpan);
    else if (dir->WordIn("right"))
      cdir[i].SetP(-wpan);

    // get incremental tilt offset
    if (dir->WordIn("up"))
      cdir[i].SetT(wtilt);
    else if (dir->WordIn("down"))
      cdir[i].SetT(-wtilt);
  }

  // make sure some valid rotation was specified (e.g. not CW)
  if (cdir[i].LenVec3() == 0.0)
    return -1;
  return 0;                  // relative
}


///////////////////////////////////////////////////////////////////////////
//                               Neck                                    //
///////////////////////////////////////////////////////////////////////////

//= Convert semantic network into a nuanced neck command and request it.
// instance number and bid already recorded by base class
// returns 1 if okay, -1 for interpretation error

int jhcSocial::ball_neck_set (const jhcAliaDesc *desc, int i)
{
  if (get_gaze(i, desc->Val("arg")) < 0)
    return -1;
  if (get_gsp(csp[i], desc->Val("arg")) < 0)
    return -1;
  ct0[i] = 0;
  return 1;
} 


//= Check whether neck command is done yet.
// sets up continuing request to body if not finished
// returns 1 if done, 0 if still working, -1 for failure

int jhcSocial::ball_neck_chk (const jhcAliaDesc *desc, int i)
{
  jhcEliNeck *n;
  double err = 0.0;

  // sanity check
  if ((rwi == NULL) || (rwi->body == NULL))
    return -1;
  n = rwi->neck;

  // determine current error (lock to sensor cycle)
  if (!rwi->Accepting())
    return 0;
  if (cdir[i].P() != 0.0)
    err = fabs(cdir[i].P() - n->Pan());
  if (cdir[i].T() != 0.0)
    err = __max(err, fabs(cdir[i].T() - n->Tilt()));

  if (cst[i] <= 0)
  {
    // record initial error
    cerr[i] = err;
    cst[i] = 1;
  }
  else
  {
    // check if finished or stuck
    jprintf(2, dbg, "neck: (%3.1f %3.1f), err = %3.1f deg, stuck = %d\n", n->Pan(), n->Tilt(), err, ct0[i]);
    if (err < ndone)
      return 1;
    if (Stuck(i, err, nprog, nstart, nmid))
      return -1;
  }

  // re-issue basic command (pan and tilt are separate resources)
  jprintf(1, dbg, ">> REQUEST %d: neck @ (%3.1f %3.1f)\n\n", cbid[i], cdir[i].P(), cdir[i].T());
  if (cdir[i].P() != 0.0)
    n->PanTarget(cdir[i].P(), csp[i], cbid[i]);
  if (cdir[i].T() != 0.0)
    n->TiltTarget(cdir[i].T(), csp[i], cbid[i]);
  return 0;
}


//= Read semantic network parts to determine desired new neck orientation.
// cdir[i] holds new (non-zero) pan and tilt values
// returns 1 if okay, -1 for problem

int jhcSocial::get_gaze (int i, const jhcAliaDesc *act) 
{
  jhcAliaDesc *dir;
  int w = 0;

  // sanity check
  if (act == NULL)
    return -1;
  cdir[i].Zero();

  // absolute position based on main verb else find direction
  if (act->WordIn("reset"))
  {
    cdir[i].SetVec3(0.1, -15.0, 0.0);       
    return 1;      
  }

  // find direction based on modifier(s)
  while ((dir = act->Fact("dir", w++)) != NULL)
  {
    // get incremental pan offset
    if (dir->WordIn("left"))
      cdir[i].SetP(npan);
    else if (dir->WordIn("right"))
      cdir[i].SetP(-npan);
    else if (dir->WordIn("straight"))
      cdir[i].SetP(0.1);

    // get incremental tilt offset
    if (dir->WordIn("up"))
      cdir[i].SetT(ntilt);
    else if (dir->WordIn("down"))
      cdir[i].SetT(-ntilt);
    else if (dir->WordIn("level"))
      cdir[i].SetT(-0.1);
  }

  // make sure some valid rotation was specified (e.g. not forward)
  if (cdir[i].LenVec3() == 0.0)
    return -1;
  return 0;        
}


//= Determine speed for gaze shift based on adverbs.
// returns 1 if proper values, -1 for problem

int jhcSocial::get_gsp (double& speed, const jhcAliaDesc *act) const
{
  const jhcAliaDesc *rate;
  int w = 0;

  // sanity check
  if (act == NULL)
    return -1;

  // look for speed modifier(s)
  speed = 1.0;
  while ((rate = act->Fact("mod", w++)) != NULL)
  {
    if (rate->WordIn("slowly"))
      speed *= sgz;            
    else if (rate->WordIn("quickly"))
      speed *= qgz;
  }
  return 1;
}
*/