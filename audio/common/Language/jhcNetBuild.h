// jhcNetBuild.h : turns parser alist into network structures
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2018-2020 IBM Corporation
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

#ifndef _JHCNETBUILD_
/* CPPDOC_BEGIN_EXCLUDE */
#define _JHCNETBUILD_
/* CPPDOC_END_EXCLUDE */

#include "jhcGlobal.h"

#include "Language/jhcMorphFcns.h"     // common audio
#include "Reasoning/jhcAliaOp.h"
#include "Reasoning/jhcAliaRule.h"
#include "Semantic/jhcGraphlet.h"

#include "Parse/jhcSlotVal.h"          


//= Turns parser alist into network structures.
// set "dbg" in base class to 1 to see call sequence on input

class jhcNetBuild : protected jhcSlotVal
{
// PROTECTED MEMBER VARIABLES
protected:
  class jhcAliaCore *core;
  jhcAliaChain *bulk;                  // fact or command


// PUBLIC MEMBER VARIABLES
public:
  // morphology (possibly shared)
  jhcMorphFcns m;

  // suggestions to add
  jhcAliaRule *rule;
  jhcAliaOp *op;


// PUBLIC MEMBER FUNCTIONS
public:
  // creation and initialization
  ~jhcNetBuild ();
  jhcNetBuild ();
  void Bind (class jhcAliaCore *all) {core = all;}

  // main functions
  void ClearLast ();
  int Assemble (const char *alist);
  

// PRIVATE MEMBER FUNCTIONS
private:
  // attention items
  int cvt_attn (const char *alist);
  double belief_val (const char *word) const;
  const char *add_evt (jhcNetNode *obj, const char *alist, 
                       jhcNodePool& pool, int neg =0, double blf =1.0) const;

  // rules
  int cvt_rule (const char *alist);
  int build_macro (const char *alist);
  int build_fwd (const char *alist);
  int build_rev (const char *alist);
  int build_ifwd (const char *alist);
  int build_sfwd (const char *alist);

  // operators
  int cvt_op (const char *alist);
  jhcAliaOp *create_op (const char **after, char *entry, const char *alist, int ssz) const;
  double pref_val (const char *word) const;
  const char *build_trig (jhcAliaOp *op, const char *entry, const char *alist) const;
  int build_proc (jhcAliaOp *op, const char *alist) const;

  // command sequences
  jhcAliaChain *build_chain (const char *alist, jhcAliaChain *final, jhcNodePool& pool) const;
  jhcAliaChain *dir_step (const char *kind) const;
  const char *build_dir (jhcGraphlet& gr, const char *entry, const char *alist, jhcNodePool& pool) const;
  jhcNetNode *query_ako (const char *alist, jhcNodePool& pool) const;
  jhcNetNode *query_hq (const char *alist, jhcNodePool& pool) const;

  // action phrases
  jhcNetNode *build_cmd (const char *alist, jhcNodePool& pool) const;
  const char *build_fact (jhcNetNode *subj, const char *alist, jhcNodePool& pool) const;
  const char *act_deg (jhcNetNode *act, const char *amt, const char *alist, jhcNodePool& pool) const;
  int add_quote (jhcNetNode *v, const char *alist, jhcNodePool& pool) const;
  jhcNetNode *add_args (jhcNetNode *v, const char *alist, jhcNodePool& pool) const;
  void add_rels (jhcNetNode *act, const char *alist, jhcNodePool& pool) const;

  // object phrases
  jhcNetNode *build_obj (const char **after, const char *alist, jhcNodePool& pool, 
                         jhcNetNode *f0 =NULL, int neg =0, double blf =1.0) const;
  void ref_props (jhcNetNode *n, jhcNodePool& pool, const char *pron, int neg) const;
  const char *obj_deg (jhcNetNode *obj, const char *deg, const char *alist, 
                       jhcNodePool& pool, int neg =0, double blf =1.0) const;
  const char *add_place (jhcNetNode *obj, char *pair, const char *alist, 
                         jhcNodePool& pool, int neg =0, double blf =1.0) const;
  const char *obj_has (jhcNetNode *obj, const char *prep, const char *alist, 
                       jhcNodePool& pool, int neg =0, double blf =1.0) const;
  const char *add_cop (jhcNetNode *obj, const char *alist, jhcNodePool& pool) const;


};


#endif  // once




