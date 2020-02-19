// jhcGraphizer.cpp : turns parser alist into network structures
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2018-2019 IBM Corporation
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

#include "Action/jhcAliaCore.h"        // common robot - only spec'd as class in header
#include "Action/jhcAliaPlay.h"

#include "Language/jhcMorphTags.h"     // common audio
#include "Language/jhcNetRef.h"

#include "Language/jhcGraphizer.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcGraphizer::~jhcGraphizer ()
{
  if (rule != NULL)
    delete rule;
  if (op != NULL)
    delete op;
}


//= Default constructor initializes certain values.

jhcGraphizer::jhcGraphizer ()
{
  rule = NULL;
  op = NULL;
}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= See if attention (to speech) should be renewed based on association list.
// mode: 0 = always, 1 = ATTN anywhere, 2 = ATTN at start, 3 = ATTN only (hail)

int jhcGraphizer::NameSaid (const char *alist, int mode) const
{
  const char *tail;

  // ignore wake-up for text input
  if (mode <= 0)
    return 1;

  // must have vocative somewhere
  if (!HasSlot(alist, "ATTN"))
    return 0;
  if (mode == 1)
    return 1;

  // must have vocative at beginning
  if ((tail = NextMatches(alist, "ATTN", 4)) == NULL)
    return 0;
  if (mode == 2)
    return 1;

  // must have no other entries
  if (*tail != '\0')
    return 0;
  return 1;
}


//= Build an appropriate structure based on given association list.
// return: 7 = farewell, 6 = greet, 5 = hail, 
//         4 = op, 3 = rule, 2 = command, 1 = fact, 
//         0 = nothing, negative for error

int jhcGraphizer::Convert (const char *alist)
{
  char entry[200];
  const char *marks;

  // sanity check then cleanup any rejected suggestions
  if (core == NULL) 
    return -2;
  if (rule != NULL)
    delete rule;
  rule = NULL;
  if (op != NULL)
    delete op;
  op = NULL;

  // check for misheard utterances
  if (alist != NULL)
  { 
    // determine if a full item has been found
    if ((marks = NextFrag(alist, entry)) != NULL)
    {
      if (strcmp(entry, "%Attn") == 0)
        return cvt_attn(marks);
      if (strcmp(entry, "%Rule") == 0)
        return cvt_rule(marks);
      if (strcmp(entry, "%Operator") == 0)
        return cvt_op(marks);
    }
    else if (HasSlot(alist, "HELLO"))
      return greet_tag();
    else if (HasSlot(alist, "BYE"))
      return farewell_tag();
    else if (HasSlot(alist, "ATTN"))   // calling robot name
      return hail_tag();
  }
  return huh_tag();                    // no network created
}


///////////////////////////////////////////////////////////////////////////
//                              Speech Acts                              //
///////////////////////////////////////////////////////////////////////////

//= Generate speech act NOTE for incomprehensible input.
// <pre>
//   NOTE[ input-1 -lex-  understand
//                 -neg-  1
//                 -agt-> self-1
//                 -obj-> user-3 ]
// </pre>
// always returns 0 for convenience

int jhcGraphizer::huh_tag () const
{
  jhcAliaAttn *attn = &(core->attn);
  jhcAliaChain *ch = new jhcAliaChain;
  jhcAliaDir *dir = new jhcAliaDir();
  jhcNetNode *n;
 
  // fill in details of the speech act
  attn->BuildIn(&(dir->key));
  n = attn->MakeNode("meta", "understand", 1);
  n->AddArg("agt", attn->self);
  n->AddArg("obj", attn->user);

  // add completed structure to attention buffer
  ch->BindDir(dir);
  attn->AddFocus(ch);
  attn->BuildIn(NULL);
  return 0;
}


//= Generate speech act noting that the robot's name was called.

int jhcGraphizer::hail_tag () const
{
  jhcAliaAttn *attn = &(core->attn);
  jhcAliaChain *ch;
  jhcNetNode *input; 

  // make a new NOTE directive
  ch = build_tag(&input, NULL);
  attn->AddLex(input, "hail");

  // add completed structure to attention buffer
  attn->AddFocus(ch);
  attn->BuildIn(NULL);
  return 5;
}


//= Generate speech act noting that the user wants to communicate.

int jhcGraphizer::greet_tag () const
{
  jhcAliaAttn *attn = &(core->attn);
  jhcAliaChain *ch;
  jhcNetNode *input; 

  // make a new NOTE directive
  ch = build_tag(&input, NULL);
  attn->AddLex(input, "greet");

  // add completed structure to attention buffer
  attn->AddFocus(ch);
  attn->BuildIn(NULL);
  return 6;
}


//= Generate speech act noting that the user is leaving.

int jhcGraphizer::farewell_tag () const
{
  jhcAliaAttn *attn = &(core->attn);
  jhcAliaChain *ch;
  jhcNetNode *input; 

  // make a new NOTE directive
  ch = build_tag(&input, NULL);
  attn->AddLex(input, "dismiss");

  // add completed structure to attention buffer
  attn->AddFocus(ch);
  attn->BuildIn(NULL);
  return 6;
}


//= Generate speech act followed by a request to add rule or operator.
// returns 3 for rule, 4 for operator

int jhcGraphizer::add_tag (int rule, const char *alist) const
{
  jhcAliaAttn *attn = &(core->attn);
  jhcAliaChain *ch, *add;
  jhcAliaDir *dir;
  jhcNetNode *input, *r;
 
  // make a new NOTE directive
  ch = build_tag(&input, alist);
  attn->AddLex(input, "give");
  r = attn->MakeNode("obj");
  input->AddArg("obj", r);
  attn->AddProp(r, "ako", ((rule > 0) ? "rule" : "operator"));

  // tack on a generic ADD directive at the end
  add = new jhcAliaChain;
  dir = new jhcAliaDir(JDIR_ADD);
  add->BindDir(dir);
  ch->cont = add;

  // add completed structure to attention buffer
  attn->AddFocus(ch);
  attn->BuildIn(NULL);
  return((rule > 0) ? 3 : 4);
}


//= Insert NOTE directive about source of command or fact before actual statement.
// gives the opportunity to PUNT and disbelief fact or reject command
// returns 2 for command, 1 for fact, 0 for problem

int jhcGraphizer::attn_tag (jhcAliaChain *bulk, const char *alist) const
{
  jhcAliaAttn *attn = &(core->attn);
  const jhcAliaDir *d0 = bulk->GetDir();
  jhcAliaChain *ch;
  jhcNetNode *input; 

  // make a new NOTE directive before bulk
  ch = build_tag(&input, alist);
  ch->cont = bulk;
  attn_args(input, bulk);

  // add completed structure to attention buffer
  attn->AddFocus(ch);
  attn->BuildIn(NULL);
  if ((d0 != NULL) && (d0->Kind() == JDIR_NOTE))     // might be play
    return 1;
  return 2;
}


//= Build a chain consisting of a single NOTE directive about speech act.
// also returns pointer to main assertion in directive
// leaves jhcGraphlet accumulator of WMEM assigned to this directive

jhcAliaChain *jhcGraphizer::build_tag (jhcNetNode **node, const char *alist) const
{
  jhcAliaAttn *attn = &(core->attn);
  jhcAliaChain *ch = new jhcAliaChain;
  jhcAliaDir *dir = new jhcAliaDir();
  jhcNetNode *n;
 
  // fill in details of the speech act
  attn->BuildIn(&(dir->key));
  n = attn->MakeNode("meta");
  n->AddArg("agt", attn->user);
  n->AddArg("dest", attn->self);
  if ((alist != NULL) && HasSlot(alist, "POLITE"))
    attn->AddProp(n, "mod", "polite");

  // embed in chain then return pieces
  ch->BindDir(dir);
  if (node != NULL)
    *node = n;
  return ch;
}


//= Add important parts of bulk as arguments to what user told.

void jhcGraphizer::attn_args (jhcNetNode *input, const jhcAliaChain *bulk) const
{
  jhcAliaAttn *attn = &(core->attn);
  const jhcAliaChain *ch = bulk;
  const jhcAliaDir *d = bulk->GetDir();
  const jhcGraphlet *g;
  jhcNetNode *n;
  JDIR_KIND kind = JDIR_MAX;
//  int i, ni = 0, ask = 0;
  int ask = 0;
  
  // mark overall type of input
  if (d != NULL)
  {
    // determine whether this is a question
    kind = d->Kind();
    if ((kind == JDIR_CHK) || (kind == JDIR_FIND))
      ask = 1;

    // get attributes of directive key
    g = &(d->key);
    n = g->Main();
//    ni = g->NumItems();
  }
  if (ask > 0)
    attn->AddLex(input, "ask");
  else
    attn->AddLex(input, "tell");

  // see if question, fact, or string of commands
  if (kind == JDIR_CHK)
     input->AddArg("ynq", n);      // yes-no question
  else if (kind == JDIR_FIND)
     input->AddArg("whq", n);      // wh- question
  else if (kind == JDIR_NOTE)
     input->AddArg("obj", attn->MakeNode("data"));
  /*
    for (i = 0; i < ni; i++)
    {
      // add all predicates to what was told (including lone lex)
      n = g->Item(i);
      if ((n->NumArgs() > 0) && ((ni == 1) || !n->LexNode()))
        input->AddArg("obj", n);
    }
  */
  else
    while (ch != NULL)
    {
      // add all explicit actions to what was requested
      if ((d = ch->GetDir()) != NULL)
        if (d->Kind() == JDIR_DO)
          input->AddArg("cmd", (d->key).Main());
      ch = ch->cont;
    }
}


///////////////////////////////////////////////////////////////////////////
//                            Attention Items                            //
///////////////////////////////////////////////////////////////////////////

//= Interpret association list to build an attention item.
// returns 1 (fact) or 2 (command) if successful, 0 for failure

int jhcGraphizer::cvt_attn (const char *alist) const
{
  char entry[200];
  jhcAliaAttn *attn = &(core->attn);
  jhcAliaDir *dir;
  jhcAliaChain *ch;

  // see if some sort of complex command
  if (NextFrag(alist, entry) != NULL)
    if ((*entry == '!') || (strcmp(entry, "%play") == 0))
    {
      if ((ch = build_chain(alist, NULL, *attn)) == NULL)
        return huh_tag();
      return attn_tag(ch, alist);
    }

  // build a single NOTE encapsulating factual assertion
  dir = new jhcAliaDir;
  attn->BuildIn(&(dir->key));
  if (build_fact(NULL, alist, *attn) == NULL)
  {
    // cleanup from error
    delete dir;
    attn->BuildIn(NULL);
    return huh_tag();
  }

  // embed NOTE in chain step
  (dir->key).MainProp();
  ch = new jhcAliaChain;
  ch->BindDir(dir);
  return attn_tag(ch, alist);
}


//= Turn qualifier ("usually") into numeric belief value.

double jhcGraphizer::belief_val (const char *word) const
{
  char term[7][20] = {"definitely", "certainly", "probably", "likely", "may", "might", "possibly"};
  double val[7]    = {   1.2,          1.1,         0.8,       0.7,     0.5,    0.5,      0.3    };
  int i;

  for (i = 0; i < 7; i++)
    if (strcmp(word, term[i]) == 0)
      return val[i];
  return 1.0;
} 


//= Add event that object was part of ("is sleeping on the bed").
// returns remainder of association list if successful

const char *jhcGraphizer::add_evt (jhcNetNode *obj, const char *alist, 
                                   jhcNodePool& pool, int neg, double blf) const
{
  char next[200];
  const char *val, *tail;
  jhcNetNode *evt;
  UL32 t;

  // make sure the next thing is some sort of event (%evt-a)
  if ((tail = NextMatches(alist, "%evt", 4)) == NULL)
    return alist;

  // find main verb then make up node representing event 
  if ((tail = FragNextPair(tail, next)) == NULL)
    return alist;
  if ((val = base_verb(t, next)) == NULL)
    return alist; 
  evt = pool.AddProp(obj, "agt", val, neg, blf, "act");
  evt->tags = t;
  
  // add complements (if any)
  while ((tail = FragNextPair(tail, next)) != NULL)
    if (SlotStart(next, "LOC") > 0)     
      tail = obj_loc(evt, next, tail, pool);           // location phrase ("at home")
    else if ((val = SlotGet(next, "HAS")) != NULL)
      tail = obj_has(evt, val, tail, pool);            // part description ("with a red top")
  return FragClose(alist);
}


///////////////////////////////////////////////////////////////////////////
//                                Rules                                  //
///////////////////////////////////////////////////////////////////////////

//= Interpret association list to build a new rule.
// returns 3 if successful, 0 for failure

int jhcGraphizer::cvt_rule (const char *alist) 
{
  char next[200];
  const char *tail;
  int ok = 0;

  // determine which primary rule pattern was used
  if ((tail = NextEntry(alist, next)) == NULL)
    return huh_tag();
  if (strcmp(next, "$macro") == 0)
    ok = build_macro(tail);
  else if (strcmp(next, "$cond") == 0)
    ok = build_fwd(tail);
  else if (strcmp(next, "$cond-i") == 0)
    ok = build_ifwd(tail);
  else if (strcmp(next, "$cond-s") == 0)
    ok = build_sfwd(tail);
  else if (strcmp(next, "$res") == 0)
    ok = build_rev(tail);
  
  // build NOTE/ADD combination
  if (ok > 0)
    return add_tag(1, alist);
  return huh_tag();
}


//= Make rule for pattern "X means Y".
// returns 1 if successful, 0 for failure

int jhcGraphizer::build_macro (const char *alist) 
{
  char pair[200], pair2[200];
  jhcNetNode *n;
  const char *tail, *wd, *wd2;

  // get two lexical terms to be related
  if ((tail = FragNextPair(alist, pair)) == NULL)
    return 0;
  if ((wd = SlotGet(pair)) == NULL)
    return 0; 
  if (FragNextPair(tail, pair2) == NULL)
    return 0;
  if ((wd2 = SlotGet(pair2)) == NULL)
    return 0; 

  // create rule structure involving two "lex" properties
  rule = new jhcAliaRule;
  rule->BuildIn(&(rule->cond));
  n = rule->MakeNode("sub", wd);
  rule->BuildIn(&(rule->result));
  rule->AddLex(n, wd2);
  return 1;
}


//= Interpret association list where condition precedes result.
// returns 1 if successful, 0 for failure

int jhcGraphizer::build_fwd (const char *alist) 
{
  const char *tail;

  // allow exit from intermediate step to cleanup
  rule = new jhcAliaRule;
  while (1)
  {
    // assemble condition part
    rule->BuildIn(&(rule->cond));
    if ((tail = build_fact(NULL, alist, *rule)) == NULL)
      break;
    (rule->cond).MainProp();

    // check for some result type
    if ((tail = NextMatches(tail, "$res")) == NULL)
     break;
    rule->BuildIn(&(rule->result));
    if (build_fact(NULL, tail, *rule) == NULL)
      break;
    return 1;
  }

  // cleanup from failure
  delete rule;
  rule = NULL;
  return 0;
}


//= Interpret association list where result precedes condition.
// returns 1 if successful, 0 for failure

int jhcGraphizer::build_rev (const char *alist) 
{
  const char *tail;
 
  // allow exit from intermediate step to cleanup
  rule = new jhcAliaRule; 
  while (1)
  {
    // assemble result part  
    rule->BuildIn(&(rule->result));
    if ((tail = build_fact(NULL, alist, *rule)) == NULL)
      break;

    // check for some condition type
    if ((tail = NextMatches(tail, "$cond")) == NULL)
      break;
    rule->BuildIn(&(rule->cond));
    if (build_fact(NULL, tail, *rule) == NULL)
      break;
    (rule->cond).MainProp();
    return 1;
  }

  // cleanup from failure
  delete rule;
  rule = NULL;
  return 0;
}


//= Interpret association list starting with an indefinite condition.
// returns 1 if successful, 0 for failure

int jhcGraphizer::build_ifwd (const char *alist) 
{
  char next[200];
  jhcNetNode *obj, *prop;
  const char *val, *tail;

  // allow exit from intermediate step to cleanup
  rule = new jhcAliaRule;
  while (1)
  {
    // assemble condition part  
    rule->BuildIn(&(rule->cond));
    if ((obj = build_obj(&tail, alist, *rule)) == NULL)
    {
      // ascribe property to unknown subject ("orange ... is a color")
      if ((tail = FragNextPair(alist, next)) == NULL)
        break;
      if ((val = SlotGet(next, "HQ")) == NULL)
        break;
      obj = rule->MakeNode("obj");
      prop = rule->AddProp(obj, "hq", val);
    }
    (rule->cond).MainProp();
    tail = FragClose(tail, 0);         // original $cond-i

    // check for some result type
    if ((tail = NextMatches(tail, "$res-i")) == NULL)
      break;
    rule->BuildIn(&(rule->result));
    if (add_cop(prop, tail, *rule) == NULL)
      break;
    return 1;
  }

  // cleanup from failure
  delete rule;
  rule = NULL;
  return 0;
}


//= Interpret association list starting with an indefinite plural condition.
// returns 1 if successful, 0 for failure

int jhcGraphizer::build_sfwd (const char *alist) 
{
  jhcNetNode *obj;
  const char *tail;

  // allow exit from intermediate step to cleanup
  rule = new jhcAliaRule;
  while (1)
  {
    // assemble condition part (no naked properties)
    rule->BuildIn(&(rule->cond));
    if ((obj = build_obj(&tail, alist, *rule)) == NULL)
      break;
    (rule->cond).MainProp();
    tail = FragClose(tail, 0);         // original $cond-s

    // check for some result type
    if ((tail = NextMatches(tail, "$res-s")) == NULL)
      break;
    rule->BuildIn(&(rule->result));
    if (build_fact(obj, tail, *rule) == NULL)
      break;
    return 1;
  }

  // cleanup from failure
  delete rule;
  rule = NULL;
  return 0;
}


///////////////////////////////////////////////////////////////////////////
//                              Operators                                //
///////////////////////////////////////////////////////////////////////////

//= Interpret association list to build a new operator.
// returns 4 if successful, 0 for failure

int jhcGraphizer::cvt_op (const char *alist) 
{
  char entry[200];
  const char *tail;

  // try to create correct kind of operator
  if ((op = create_op(&tail, entry, alist, 200)) == NULL)
    return huh_tag();

  // fill in parts
  if ((tail = build_trig(op, entry, tail)) != NULL)
    if (build_proc(op, tail) > 0)
      return add_tag(0, alist);

  // cleanup from some problem
  delete op;
  op = NULL;
  return huh_tag();
}


//= Create a new operator with  appropriate type of trigger condition (blank for now).
// "after" points to part of "alist" which has not been used yet, entry gets full directive request
// if trigger is a prohibition then cond type will be ANTE and default PUNT chain is added
// returns new operator or NULL for problem

jhcAliaOp *jhcGraphizer::create_op (const char **after, char *entry, const char *alist, int ssz) const
{
  char val[200];
  jhcAliaDir dcvt;
  jhcAliaOp *op;
  const char *tail;
  JDIR_KIND k = JDIR_NOTE;
  int veto = 0;

  // make sure operator begins with a triggering condition
  *after = alist;
  if ((tail = FindFrag(alist, "$trig-n")) != NULL)
    veto = 1;
  else if ((tail = FindFrag(alist, "$trig")) == NULL)
    return NULL;
  *after = tail;

  // base directive type on first command found (default = NOTE)
  while ((tail = FragNextFrag(tail, entry, ssz)) != NULL)
    if (*entry == '!')
    {
      if ((k = dcvt.CvtKind(entry + 1)) >= JDIR_MAX)
        return NULL;
      if ((k == JDIR_DO) && (veto > 0))    // prohibitions are ANTE
        k = JDIR_ANTE;                     
      break;
    }

  // create operator of proper kind and adjust preference
  op = new jhcAliaOp(k);
  if (FindSlot(alist, "PREF", val) != NULL)
    op->pref = pref_val(val);

  // possibly add final PUNT directive for prohibitions
  if (veto > 0)
    op->meth = dir_step("punt");
  return op;
}


//= Turn qualifier ("should") into numeric belief value.

double jhcGraphizer::pref_val (const char *word) const
{
  char term[5][20] = {"might", "could", "should", "must", "always"};
  double val[5]    = {  0.3,     0.5,      1.2,    1.5,      2.0  };
  int i;

  for (i = 0; i < 5; i++)
    if (strcmp(word, term[i]) == 0)
      return val[i];
  return 1.0;
} 


//= Fill in bulk of trigger condition for an operator.
// returns part of "alist" which has not been used yet if successful, NULL for problem

const char *jhcGraphizer::build_trig (jhcAliaOp *op, const char *entry, const char *alist) const
{
  const char *tail = alist;

// could have "unless" parts also

  // strip leading directive if needed
  if (*entry == '!')
    tail = NextEntry(alist, NULL, 0);              
  return build_dir(op->cond, entry, tail, *op);
}


//= Assemble chain of actions for procedure part of operator.
// returns 1 if okay, 0 if some problem

int jhcGraphizer::build_proc (jhcAliaOp *op, const char *alist) const
{
  const char *tail;

  if ((tail = FindFrag(alist, "$proc")) == NULL)
    return 1;
  if ((op->meth = build_chain(tail, op->meth, *op)) == NULL)
    return 0;
  return 1;
}  


//= Create structure for finding out the type of something.

jhcNetNode *jhcGraphizer::query_ako (const char *alist, jhcNodePool& pool) const
{
  jhcNetNode *obj;
  const char *tail;

  // get referent and add unknown AKO property
  if ((obj = build_obj(&tail, alist, pool)) == NULL)
    return NULL;
  return pool.AddProp(obj, "ako", NULL);
}


//= Create structure for finding out some property of something.

jhcNetNode *jhcGraphizer::query_hq (const char *alist, jhcNodePool& pool) const
{
  char entry[200];
  jhcNetNode *obj, *main;
  const char *tail, *t2, *kind;

  // get desired property kind
  if ((tail = NextEntry(alist, entry)) == NULL)
    return NULL;
  if ((kind = SlotGet(entry, "AKO")) == NULL)
    return NULL;

  // get referent then add unknown HQ of given kind
  if ((obj = build_obj(&t2, tail, pool)) == NULL)
    return NULL;
  main = pool.AddProp(obj, "hq", NULL);
  pool.AddProp(main, "ako", kind);
  return main;
}


///////////////////////////////////////////////////////////////////////////
//                           Command Sequences                           //
///////////////////////////////////////////////////////////////////////////

//= Create a chain of activities, some sequential others potentially parallel.
// will append "final" activity (can be NULL) to full chain built 
// returns NULL if some problem

jhcAliaChain *jhcGraphizer::build_chain (const char *alist, jhcAliaChain *final, jhcNodePool& pool) const
{
  char entry[200];
  jhcAliaPlay *play = NULL;
  jhcAliaChain *pod, *ch0, *start = NULL, *ch = NULL;
  jhcAliaDir *dir = NULL;
  const char *tail = alist;

  // handle sequence of actions in chain
  while ((tail = NextFrag(tail, entry)) != NULL)
    if (strcmp(entry, "%play") == 0)
    {
      // make the next chain step be a new play
      pod = new jhcAliaChain;
      play = new jhcAliaPlay;
      pod->BindPlay(play);
      if (ch == NULL)
        start = pod;
      else
        ch->cont = pod;  
      ch = NULL;
    }
    else if ((strcmp(entry, "%") == 0) && (play != NULL))
    {
      // finish off any current play
      ch = pod;
      play = NULL; 
    }
    else if (*entry == '!')
    {
      // make up new chain step which is a single directive
      ch0 = ch;
      if ((ch = dir_step(entry + 1)) == NULL)
        break;
      dir = ch->GetDir();

      // complete action spec, set dir to NULL if success 
      if ((tail = build_dir(dir->key, entry, tail, pool)) == NULL)
        break;
      dir = NULL;

      // add either as a required activity or tack onto end of chain
      if (play != NULL)
        play->AddReq(ch);
      else if (ch0 == NULL)
        start = ch;
      else
        ch0->cont = ch;  
      tail = FragClose(tail, 0);
    }

  // check for success 
  if (dir == NULL)
  {
    ch->cont = final;
    return start;
  }

  // cleanup (chain automatically deletes payload)
  delete start;                       
  return NULL;
}


//= Create a new chain step consisting of a directive with some kind.

jhcAliaChain *jhcGraphizer::dir_step (const char *kind) const
{
  jhcAliaDir *dir = new jhcAliaDir;
  jhcAliaChain *ch;

  // make sure directive kind is valid
  if (dir->SetKind(kind) <= 0)
  {
    delete dir;
    return NULL;
  }

  // embed directive in new chain step
  ch = new jhcAliaChain;
  ch->BindDir(dir);
  return ch;
}


//= Fill in details of directive from remaining association list.
// returns unused portion of list

const char *jhcGraphizer::build_dir (jhcGraphlet& gr, const char *entry, const char *alist, jhcNodePool& pool) const
{
  jhcNetNode *main;
  const char *tail = alist;

  // make new nodes in specified graphlet
  pool.BuildIn(&gr);

  // try building structure corresponding to a command 
  if (strcmp(entry, "!find-ako") == 0)
    main = query_ako(alist, pool);
  else if (strcmp(entry, "!find-hq") == 0)
    main = query_hq(alist, pool);
  else if ((main = build_cmd(alist, pool)) == NULL)        // try real verb
  { 
    // build copula sentence ("something is close" as trigger)
    if ((main = build_obj(&tail, alist, pool)) == NULL)
      return NULL;
    if ((tail = add_cop(main, tail, pool)) == NULL)
      return NULL;
    main = gr.MainProp();
  }

  // if success highlight most important part of graphlet
  if (main == NULL)
    return NULL;
  gr.SetMain(main);
  return tail;
}


///////////////////////////////////////////////////////////////////////////
//                             Action Phrases                            //
///////////////////////////////////////////////////////////////////////////

//= Create network structure for imperative verb phrase.
// returns pointer to newly created action node

jhcNetNode *jhcGraphizer::build_cmd (const char *alist, jhcNodePool& pool) const
{
  char next[200];
  const char *end, *t2, *tail = alist, *val = NULL;
  jhcNetNode *agt, *act;
  UL32 t = 0;      
  int quote = 0, neg = 0;  


  // strip directive if first item then check for overall negation of verb
  if ((t2 = NextEntry(tail, next)) != NULL)
    if (*next == '!')
      tail = t2;
  if (FragHasSlot(tail, "NEG-V"))
    neg = 1;

  // look for main verb but ignore placeholder "do something"
  while ((tail = FragNextPair(tail, next)) != NULL)
    if ((val = base_verb(t, next)) != NULL)                
      break;
  if (val == NULL)
    return NULL;
  if (strcmp(val, "do something") == 0)
    val = NULL;

  // see if special "say" type then make node for action
  if (strncmp(next, "SAY", 3) == 0)
    quote = 1;
  end = tail;
  act = pool.MakeNode("act", val, neg);
  act->tags = t;

  // go back and see if some object at front 
  if ((agt = build_obj(&t2, alist, pool)) != NULL)
    act->AddArg("agt", agt);

  // attach all adverbial modifiers
  tail = alist;
  while ((tail = FragNextPair(tail, next)) != NULL)
    if ((val = SlotGet(next, "DEG")) != NULL)
      tail = act_deg(act, val, tail, pool);
    else if ((val = SlotGet(next, "MOD")) != NULL)
      pool.AddProp(act, "mod", val);
    else if ((val = SlotGet(next, "AMT")) != NULL)
      pool.AddProp(act, "amt", val);
    else if ((val = SlotGet(next, "DIR")) != NULL)
      pool.AddProp(act, "dir", val);

  // add noun-like arguments or quoted string 
  if (quote > 0)
    add_quote(act, end, pool);
  else
    act = add_args(act, end, pool);
  return act;
}


//= Build a sentence-like semantic network with subject and object(s).
// can optionally take pre-defined subject with alist being rest of statement

const char *jhcGraphizer::build_fact (jhcNetNode *subj, const char *alist, jhcNodePool& pool) const
{
  char next[200];
  const char *t2, *after = alist, *tail = alist, *val = NULL;
  jhcNetNode *act, *agt = subj;
  UL32 t = 0;      
  int neg = 0;  

  // see if copula vs. sentence with verb
  if (HasFrag(alist, "$add")) 
  {
    // build structure for add-on features ("is nice")
    if (agt == NULL)
      if ((agt = build_obj(&tail, alist, pool)) == NULL)
        return NULL;
    tail = add_cop(agt, tail, pool);   // end of original $add
    return FragClose(tail, 0);         // end of enclosing fragment
  }

  // check for overall negation of verb
  if (FragHasSlot(alist, "NEG-V"))
    neg = 1;

  // look for main verb and make node for sentence
  while ((after = FragNextPair(after, next)) != NULL)
    if ((val = base_verb(t, next)) != NULL)                
      break;
  if (val == NULL)
    return NULL;
  act = pool.MakeNode("act", val, neg);
  act->tags = t;

  // go back and see if some object at front 
  if (agt == NULL)
    agt = build_obj(&t2, alist, pool);
  if (agt != NULL)
    act->AddArg("agt", agt);

  // attach all adverbial modifiers (anywhere in sentence)
  while ((tail = FragNextPair(tail, next)) != NULL)
    if ((val = SlotGet(next, "DEG")) != NULL)
      tail = act_deg(act, val, tail, pool);
    else if ((val = SlotGet(next, "MOD")) != NULL)
      pool.AddProp(act, "mod", val);
    else if ((val = SlotGet(next, "AMT")) != NULL)
      pool.AddProp(act, "amt", val);
    else if ((val = SlotGet(next, "DIR")) != NULL)
      pool.AddProp(act, "dir", val);

  // add noun-like arguments or quoted string (after verb)
  act = add_args(act, after, pool);
  return FragClose(after, 0);          // end of enclosing fragment
}


//= Applies standard morphology to value in supplied verb in entry (altered).
// also sets "tags" to proper morphology bits
// returns canonical version of value (present)
// NOTE: should be part of morphology class

char *jhcGraphizer::base_verb (UL32& tags, char *pair) const
{
  char *val;
  int end;

  // check for verbatim echo
  if (strncmp(pair, "SAY", 3) == 0)
  {
    if ((val = strchr(pair, '=')) == NULL)
      return NULL;
    tags = JTAG_VIMP;
    return(val + 1);
  }

  // check for proper slot prefix then get default value
  if (strncmp(pair, "ACT", 3) != 0)
    return NULL;
  if ((val = strchr(pair, '=')) == NULL)
    return NULL;
  val++;
  end = (int) strlen(val);

  // check for known verb forms
  tags = JTAG_VIMP;
  if (strncmp(pair, "ACT-S", 5) == 0)
  {
    // active verb ending in -s
    tags = JTAG_VPRES;
    if ((end > 0) && (val[end - 1] == 's'))
      val[end - 1] = '\0';
  }
  else if (strncmp(pair, "ACT-D", 5) == 0)
  {
    // past tense verb ending in -ed
    tags = JTAG_VPAST;
    if ((end > 2) && (val[end - 2] == 'e') && (val[end - 1] == 'd'))                 
    {
      // doubled consonant ("grabbed")
      if ((end > 4) && (val[end - 4] == val[end - 3]))
        val[end - 3] = '\0';
      else
        val[end - 2] = '\0';
    }
  }
  else if (strncmp(pair, "ACT-G", 5) == 0)
  {
    // progressive verb ending in -ing
    tags = JTAG_VPROG;
    if ((end > 3) && (val[end - 3] == 'i') && (val[end - 2] == 'n') && (val[end - 1] == 'g'))                 
    {
      // doubled consonant ("tipping")
      if ((end > 5) && (val[end - 5] == val[end - 4]))
        val[end - 4] = '\0';
      else
        val[end - 3] = '\0';
    }
  }
  return val;
}


//= Make nodes for adverbial descriptions with a degree ("very slowly").
// returns unused portion of alist

const char *jhcGraphizer::act_deg (jhcNetNode *act, const char *amt, const char *alist, jhcNodePool& pool) const
{
  char pair[200];
  jhcNetNode *prop;
  const char *val, *tail;

  // possibly add an adverbial description to node
  if ((tail = FragNextPair(alist, pair)) == NULL)
    return alist;
  if ((val = SlotGet(pair, "MOD")) == NULL)
    return alist;

  // modify the adjectival descriptor (override pool defaults)
  prop = pool.AddProp(act, "mod", val);
  pool.AddProp(prop, "deg", amt);
  return tail;
}


//= Look for direct object (or infinitive) plus indirect object and link them to verb.
// returns embedded infinitive command (if any), else main verb (passed in)

jhcNetNode *jhcGraphizer::add_args (jhcNetNode *v, const char *alist, jhcNodePool& pool) const
{
  char entry[200];
  const char *t2, *tail = alist;
  jhcNetNode *obj2, *dobj = NULL, *iobj = NULL, *act = NULL;
  int n = 0;

  // sanity check 
  if (alist == NULL)
    return 0;

  // look for direct object
  if ((dobj = build_obj(&t2, tail, pool)) != NULL)
  {
    // look for indirect object
    tail = t2;
    n++;
    if ((obj2 = build_obj(&t2, tail, pool)) != NULL)
    {
      // correct order is iobj then dobj 
      tail = t2;  
      iobj = dobj;
      dobj = obj2;
      n++;
    }
    else if ((t2 = NextFrag(tail, entry)) != NULL)
      if (strcmp(entry, "!do") == 0)
        if ((act = build_cmd(t2, pool)) != NULL)
        {
          // correct order is iobj then infinitive
          tail = t2;
          iobj = dobj;
          dobj = NULL;
          n++;
        }
  }
        
  // attach arguments (if any)
  if (iobj != NULL)
    v->AddArg("dest", iobj);
  if (dobj != NULL)
    v->AddArg("obj", dobj);
  if (act != NULL)
    v->AddArg("cmd", act);
  return((act != NULL) ? act : v);
}


//= Add a node which has a long literal string expansion.
// returns 1 if found, 0 for problem

int jhcGraphizer::add_quote (jhcNetNode *v, const char *alist, jhcNodePool& pool) const
{
  char next[200];
  jhcNetNode *q;
  const char *val, *tail = alist;

  while ((tail = FragNextPair(tail, next)) != NULL)
    if ((val = SlotGet(next, "QUOTE", 0)) != NULL)
    {
      q = pool.MakeNode("txt");
      q->SetString(val);
      v->AddArg("obj", q);
      return 1;
    }
  return 0;
}


///////////////////////////////////////////////////////////////////////////
//                             Object Phrases                            //
///////////////////////////////////////////////////////////////////////////

//= Create network structure for noun phrase.
// initially creates description in a NodeRef to check if a referent already exists
// can optionally force new description onto an old object in pool by setting "f0"
// returns pointer to newly created object node (and advances alist to after object)

jhcNetNode *jhcGraphizer::build_obj (const char **after, const char *alist, 
                                     jhcNodePool& pool, jhcNetNode *f0, double blf) const
{
  jhcNetRef nr;
  char next[200];
  const char *val, *tail;
  jhcNetNode *obj, *act;
  UL32 t;
  int find = 1;

  // make sure the next thing is some sort of object description
  tail = NextEntry(alist, next);
  if (strncmp(next, "%obj", 4) != 0)
    return NULL;
  if (strncmp(next, "%obj-i", 6) == 0)
    find = 0;                                                  // no new node required    

  // add features to object node in temporary network                  
  obj = nr.MakeNode("obj");
  while ((tail = FragNextPair(tail, next)) != NULL)
    if ((val = SlotGet(next, "REF", 0)) != NULL)               // reference ("you", "she")
      ref_props(obj, nr, val);
    else if ((val = SlotGet(next, "NAME", 0)) != NULL)         // proper noun ("Jim")
      nr.AddLex(obj, val, 0, blf);                  
    else if ((val = base_noun(obj->tags, next)) != NULL)       // base type ("dog") 
      nr.AddProp(obj, "ako", val, 0, blf);                   
    else if ((val = SlotGet(next, "HQ")) != NULL)              // simple property ("big") 
      nr.AddProp(obj, "hq", val, 0, blf);                    
    else if ((val = SlotGet(next, "DEG")) != NULL)             // degree property ("very red")
      tail = obj_deg(obj, val, tail, nr, 0, blf);            
    else if (SlotStart(next, "ACT-G") > 0)                     // participle ("sleeping")
    {
      act = nr.AddProp(obj, "agt", base_verb(t, next), 0, blf, "act");
      act->tags = t;
    }
    else if (SlotStart(next, "LOC") > 0)                       // location phrase ("at home")
      tail = obj_loc(obj, next, tail, nr, 0, blf);           
    else if ((val = SlotGet(next, "HAS")) != NULL)             // part description ("with a red top")
      tail = obj_has(obj, val, tail, nr, 0, blf);            

  // possibly link to existing node else create new graph
  if (after != NULL)
    *after = FragClose(alist);
  return nr.FindMake(pool, find, f0);
}


//= Add properties to object node based on pronoun used for reference.

void jhcGraphizer::ref_props (jhcNetNode *n, jhcNodePool& pool, const char *pron) const
{
  if (strcmp(pron, "you") == 0)
    pool.AddLex(n, "you");
  else if ((strcmp(pron, "me") == 0) || (_stricmp(pron, "I") == 0))
    pool.AddLex(n, pron);
  else if ((strcmp(pron, "he") == 0) || (strcmp(pron, "him") == 0))
    pool.AddProp(n, "hq", "male");
  else if ((strcmp(pron, "she") == 0) || (strcmp(pron, "her") == 0))
    pool.AddProp(n, "hq", "female");
}


//= Applies standard morphology to value in supplied noun in entry (altered).
// also sets "tags" to proper morphology bits
// returns canonical version of value (singular)
// NOTE: should be part of morphology class

char *jhcGraphizer::base_noun (UL32& tags, char *pair) const
{
  char dummy[2][40] = {"thing", "something"};
  char *val;
  int i, end;

  // check for proper slot prefix then get default value
  if (strncmp(pair, "AKO", 3) != 0)
    return NULL;
  if ((val = strchr(pair, '=')) == NULL)
    return NULL;
  ++val;
  end = (int) strlen(val) - 1;

  // check for special non-informative words
  tags = 0;
  for (i = 0; i < 2; i++)
    if (strcmp(val, dummy[i]) == 0)
      return NULL;

  // see if this is a plural ending in -s
  if (strncmp(pair, "AKO-S", 5) == 0)
  {
    // mark as plural
    tags = JTAG_NPL;
    if ((end > 0) && (val[end] == 's'))
    {
      // remove final -s then check for s-es and i-es cases
      val[end] = '\0';
      if ((end > 2) && (val[end - 2] == 's') && (val[end - 1] == 'e'))
        val[end - 1] = '\0';   
      else if ((end > 2) && (val[end - 2] == 'i') && (val[end - 1] == 'e'))
      {
        val[end - 1] = '\0';
        val[end - 2] = 'y';
      } 
    }
  }
  return val;
}


//= Make nodes for adjectival descriptions with a degree ("very red").
// returns unused portion of alist

const char *jhcGraphizer::obj_deg (jhcNetNode *obj, const char *amt, const char *alist, 
                                   jhcNodePool& pool, int neg, double blf) const
{
  char pair[200];
  jhcNetNode *prop, *mod;
  const char *val, *tail;

  // figure out what kind of relation is being given a degree
  if ((tail = FragNextPair(alist, pair)) == NULL)
    return alist;

  // modify a new adjectival descriptor 
  if ((val = SlotGet(pair, "HQ")) != NULL)
  {
    prop = pool.AddProp(obj, "hq", val, neg, blf);
    mod = pool.AddProp(prop, "deg", amt);
    return tail;
  }

  // modify a location descriptor (gets most recent "loc" property)
  if ((val = SlotGet(pair, "LOC")) != NULL)
  {
    tail = obj_loc(obj, pair, tail, pool, neg, blf);
    prop = obj->Fact("loc", obj->NumFacts("loc") - 1);   
    mod = pool.AddProp(prop, "deg", amt);
    return tail;
  }
  return alist;
}


//= Make nodes for location phrases ("at home" or "between here and there").
// returns unused portion of alist

const char *jhcGraphizer::obj_loc (jhcNetNode *obj, char *pair, const char *alist, 
                                   jhcNodePool& pool, int neg, double blf) const
{
  jhcNetNode *ref, *prop;
  const char *t2, *tail, *rel = SlotGet(pair, "LOC");

  // check for required anchor object
  if ((ref = build_obj(&tail, alist, pool)) == NULL)
    return alist;
  prop = pool.AddProp(obj, "loc", rel, neg, blf);
  prop->AddArg("wrt", ref);                        // add reference object 

  // check if second anchor expected
  if (SlotStart(pair, "LOC-2") <= 0)
    return tail;
  if ((ref = build_obj(&t2, tail, pool)) == NULL)
    return tail;
  prop->AddArg("wrt", ref);                        // add second reference  
  return t2;
}


//= Make nodes for part phrases ("with a red top").
// returns unused portion of alist

const char *jhcGraphizer::obj_has (jhcNetNode *obj, const char *prep, const char *alist, 
                                   jhcNodePool& pool, int neg, double blf) const
{
  jhcNetNode *part, *prop;
  const char *tail;

  // check for required part
  if ((part = build_obj(&tail, alist, pool)) == NULL)
    return alist;
  prop = pool.AddProp(obj, "has", prep, neg, blf);
  prop->AddArg("obj", part); 
  return tail;
}


//= Check for copula tail end (e.g. "is nice") and add features to node.
// features added directly since never need to check for a reference for this description
// returns unused portion of alist

const char *jhcGraphizer::add_cop (jhcNetNode *obj, const char *alist, jhcNodePool& pool) const
{
  char next[200];
  const char *val, *post, *tail;
  double blf = 1.0;                     // was 0.0
  int neg = 0;

  // if following part is an addition then get first pair
  if ((tail = NextMatches(alist, "$add", 4)) == NULL)
    return alist;
  post = tail;                                           // after all slot-value pairs

  // go through all the pairs in this fragment
  while ((tail = FragNextPair(tail, next)) != NULL)
  {
    if ((val = SlotGet(next, "BLF")) != NULL)            // overall belief ("usually")
      blf = belief_val(val);                             
    else if (SlotStart(next, "NEG") > 0)                 // overall negation ("not")
      neg = 1;                                           
    else if ((val = SlotGet(next, "NAME", 0)) != NULL)   // proper name ("Groot")
      pool.AddProp(obj, "lex", NULL, neg, blf, val);     
    else if ((val = SlotGet(next, "HQ")) != NULL)        // simple property ("big") 
      pool.AddProp(obj, "hq", val, neg, blf);            
    else if ((val = SlotGet(next, "DEG")) != NULL)       // degree property ("very red")
      tail = obj_deg(obj, val, tail, pool, neg, blf);    
    else if (SlotStart(next, "LOC") > 0)                 // location phrase ("at home")    
      tail = obj_loc(obj, next, tail, pool, neg, blf);   
    post = tail;
  }

  // check for indeterminate predicate nominal ("a dog")
  if (build_obj(NULL, post, pool, obj, blf) == NULL)
    add_evt(obj, post, pool, neg, blf);
  return FragClose(alist);
}


///////////////////////////////////////////////////////////////////////////
//                           Debugging Functions                         //
///////////////////////////////////////////////////////////////////////////

//= Print out representation of last thing built from parsing output.

void jhcGraphizer::PrintLast ()
{

}

