// jhcMotRamp.cpp : trapezoidal velocity profiling for 3D vectors
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2016-2019 IBM Corporation
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

#include "Interface/jhcMessage.h"

#include "Geometry/jhcMotRamp.h"

#include <string.h>
///////////////////////////////////////////////////////////////////////////
//                       Creation and Configuration                      //
///////////////////////////////////////////////////////////////////////////

//= Default constructor initializes certain values.

jhcMotRamp::jhcMotRamp () 
{
  cmd.SetSize(4);
  *rname = '\0';
  RampCfg(); 
  RampReset();
}


///////////////////////////////////////////////////////////////////////////
//                             Servo Control                             //
///////////////////////////////////////////////////////////////////////////

//= Give a motion control stop value based on current real value.
// move speed can be read by RampVel()

double jhcMotRamp::RampNext (double now, double tupd, double lead)
{
  jhcMatrix stop(4), loc(4);

  loc.SetVec3(now, 0.0, 0.0);
  RampNext(stop, loc, tupd, lead);
  return stop.X();
}


//= Give a motion control target stop position and based on current real position.
// "tupd" is the time since last call, presumed to be approximate time to next call also

void jhcMotRamp::RampNext (jhcMatrix& stop, const jhcMatrix& now, double tupd, double lead) 
{
  double f;
//double d00 = d0, slow = 0.2;

  // make sure vectors are okay
  if (!stop.Vector(4) || !now.Vector(4) || (tupd <= 0.0))
    Fatal("Bad input to jhcMotRamp::RampNext");

  // find distance to goal and check if progress is being made
  dist = find_dist(now, cmd);
  if ((d0 - dist) > fabs(done))
  {
    d0 = dist;
    stuck = 0.0;
  }
  else
  {
    d0 = __max(d0, dist);
    stuck += tupd;
  }

/*
//if (*rname != '\0')
if ((strcmp(rname, "hand_ramp") == 0) || (strcmp(rname, "dir_ramp") == 0))
jprintf("%c %s: dist0 = %3.1f, dist = %3.1f -> diff = %+4.2f, expect >= %4.2f\n", 
((stuck <= 0.0) ? '+' : ' '), rname, 
d00, dist, d00 - dist, fabs(done));
*/

  // unusual case of being exactly at goal
  if (dist == 0.0)
  {
    sp = 0.0;
    stop.Copy(now);
    return;
  }

  // get new path speed and fraction to reduce distance
  sp = pick_sp(sp, dist, tupd);
  f = sp * tupd * lead / dist;
  f = __min(f, 1.0);

  // move along the difference vector
  if (done < 0)
    stop.CycMix3(now, cmd, f);
  else
    stop.MixVec3(now, cmd, f);

/*
char txt[80];
if ((strcmp(rname, "hand_ramp") == 0) || (strcmp(rname, "dir_ramp") == 0))
jprintf("    dist = %3.1f --> sp = %3.1f [rt = %4.2f, f = %4.2f] -> %s\n", dist, sp, rt, f, stop.ListVec3(txt));
*/
}


//= Pick new "movingness" path speed based on current speed and distance to goal.
// "rt" determines acceleration and top speed, ignores inertia and deceleration limit
// makes sure that limited acceleration will cause stop at goal "cmd"
// <pre>
//        ^
//     sp |       +-----------
//        |     /
//        |   /
//        | /
//       -+------------------->
//                       dist
// </pre>

double jhcMotRamp::pick_sp (double v0, double dist, double tupd) const
{
  double a, d, vmax, vstop, v;

  // compute commanded accelerations and cruise speed
  a = ((rt < 0.0) ? astd : rt * rt * astd);
  d = ((rt < 0.0) ? dstd : rt * rt * dstd);
  vmax = fabs(rt) * vstd;

  // determine final deceleration limited target speed
  vstop = sqrt(2.0 * d * dist);
  vmax = __min(vstop, vmax);

  // adjust speed with initial acceleration but keep below limit
  v = v0 + a * tupd;
  v = __min(v, vmax);
  return v;
}


//= Generate component-wise error vector between current and target positions.
// compares remembered last position and current command

void jhcMotRamp::RampErr (jhcMatrix& err, const jhcMatrix& loc, int abs) const
{
  if (!err.Vector(4))
    Fatal("Bad input to jhcMotRamp::RampErr");
  if (done < 0) 
    err.CycDiff3(loc, cmd); 
  else 
    err.DiffVec3(loc, cmd);
  if (abs > 0)
    err.Abs();
}


///////////////////////////////////////////////////////////////////////////
//                          Trajectory Queries                           //
///////////////////////////////////////////////////////////////////////////

//= Estimate time (in secs) to move a certain distance using given rate.
// if rate < 0 then does not scale acceleration (for snappier response)
// assumes velocity starts and stops at zero

double jhcMotRamp::find_time (double dist, double rate) const
{
  double v, ad, t, r = fabs(rate);
  
  // limit velocity and possibly acceleration too
  v = fabs(rate) * vstd;
  ad = 2.0 * astd * dstd / (astd + dstd);
  if (rate > 0.0)
    ad *= (r * r);

  // see if triangular or trapezoidal profile
  if (dist <= (v * v / ad))
    t = 2.0 * sqrt(dist / ad);
  else
    t = (dist / v) + (v / ad);
  return t;
}


//= Pick a rate to move a certain distance in the given time.
// if secs < 0 then does not scale acceleration (for snappier response)

double jhcMotRamp::find_rate (double dist, double secs, double rmax) const
{
  double r, v, t2, ad = 2.0 * astd * dstd / (astd + dstd), t = fabs(secs);

  // simple case (wishful thinking)
  if (t == 0.0)
    return((secs > 0.0) ? rmax : -rmax);

  // see if both acceleration and velocity should be scaled
  if (secs > 0.0)
  {
    // check if triangular (covers distance at half peak velocity)
    v = 2.0 * dist / t;
    if (v > (rmax * vstd))
      r = ((dist / vstd) + (vstd / ad)) / t;     // trapezoidal 
    else
      r = v / sqrt(ad * dist);                   // triangular
  }
  else          
  {
    // compute time required for triangular profile to reach goal
    t2 = 2.0 * sqrt(dist / ad);
    v = 2.0 * dist / t2;

    // possibly compute cruise speed for trapezoidal profile instead
    if ((t2 < t) || (v > (rmax * vstd)))
      v = 0.5 * ad * t * (1.0 - sqrt(1.0 - (4.0 * dist / (ad * t * t))));
    r = v / vstd;
  }

  // clamp rate and set sign
  r = __min(r, rmax);
  return((secs > 0.0) ? r : -r);
}


//= Find difference between two vector values.

double jhcMotRamp::find_dist (const jhcMatrix& p2, const jhcMatrix& p1) const
{
  if (done < 0)
    return p2.RotDiff3(p1);
  return p2.PosDiff3(p1);
}


//= Find difference between two scalar values.

double jhcMotRamp::find_dist (double p2, double p1) const
{
  double d = p2 - p1;

  if (done < 0)
  {
    while (d > 180.0)
      d -= 360.0;
    while (d <= -180.0)
      d += 360.0;
  }
  return fabs(d);
}
