// jhcMorphFcns.h : converts words from base form plus tag to surface form
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 IBM Corporation
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

#ifndef _JHCMORPHFCNS_
/* CPPDOC_BEGIN_EXCLUDE */
#define _JHCMORPHFCNS_
/* CPPDOC_END_EXCLUDE */

#include "jhcGlobal.h"

#include "Parse/jhcSlotVal.h"


//= Converts words from base form plus tag to surface form.
// some more mainstream stemmer package like Snowball might be used

class jhcMorphFcns : private jhcSlotVal
{
// PRIVATE MEMBER VARIABLES
private:
  // number of exceptions for each POS
  static const int nmax = 100; 
  static const int amax = 100; 
  static const int vmax = 100; 

  // lookup tables for irregular forms
  char nsing[nmax][40], npl[nmax][40]; 
  char vimp[vmax][40], vpres[vmax][40], vprog[vmax][40], vpast[vmax][40];
  char adj[amax][40], comp[amax][40], sup[amax][40];
  int nn, nv, na;


// PUBLIC MEMBER VARIABLES
public:


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  ~jhcMorphFcns ();
  jhcMorphFcns ();

  // exceptions
  void ClrExcept ();
  int LoadExcept (const char *fname);
 
  // normalization functions
  const char *BaseNoun (UL32& tags, char *pair) const;
  const char *BaseVerb (UL32& tags, char *pair) const;
  const char *BaseAdj (UL32& tags, char *pair) const;

  // utilities
  int LexDeriv (const char *gram) const;
  int LexBase (const char *deriv) const;


// PRIVATE MEMBER FUNCTIONS
private:
  // word functions
  const char *noun_stem (char *val, UL32 tags) const;
  const char *verb_stem (char *val, UL32 tags) const;
  const char *adj_stem (char *val, UL32 tags) const;
  UL32 gram_tag (const char *pair) const;

  // conversions
  const char *get_base (const char *surf, UL32 tags) const;
  bool vowel (char c) const;
  const char *get_surface (const char *surf, UL32 tags);
  const char *scan_for (const char *probe, const char key[][40], const char val[][40], int n) const;


};


#endif  // once




