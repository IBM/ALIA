// jhcMorphFcns.cpp : does something
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

#include <string.h>

#include "Language/jhcMorphTags.h"

#include "Language/jhcMorphFcns.h"


///////////////////////////////////////////////////////////////////////////
//                      Creation and Initialization                      //
///////////////////////////////////////////////////////////////////////////

//= Default destructor does necessary cleanup.

jhcMorphFcns::~jhcMorphFcns ()
{

}


//= Default constructor initializes certain values.

jhcMorphFcns::jhcMorphFcns ()
{

}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Clear all exceptions to morphology rules.

void jhcMorphFcns::ClrExcept ()
{
  nn = 0;
  nv = 0;
  na = 0;
}


//= Load some exceptions to morphology rules (generally appends).
// returns positive if successful, 0 or negative for problem

int jhcMorphFcns::LoadExcept (const char *fname)
{

  return 1;
}
 

///////////////////////////////////////////////////////////////////////////
//                        Normalization Functions                        //
///////////////////////////////////////////////////////////////////////////

//= Applies standard morphology to value in supplied noun in pair (altered!).
// pair input is part of an association list with a part of speech like "AKO-S=birds"
// also sets "tags" to proper morphology bits as a mask (not value)
// returns canonical version of value (singular) or NULL if not a noun
// NOTE: string might be a temporary array which can be rewritten (use soon)

const char *jhcMorphFcns::BaseNoun (UL32& tags, char *pair) const
{
  char dummy[2][40] = {"thing", "something"};
  char *val = SlotRef(pair);
  const char *irr;
  int i;

  // check for special non-informative words
  if (val == NULL)
    return NULL;
  for (i = 0; i < 2; i++)
    if (strcmp(val, dummy[i]) == 0)
      return NULL;

  // check for exception otherwise strip suffix
  tags = gram_tag(pair);
  if ((tags & JTAG_NOUN) == 0)
    return NULL;
  if ((irr = get_base(val, tags)) != NULL)
    return irr;
  return noun_stem(val, tags);
}

 
//= Strip plural suffixes from surface word to get base noun.
// returns pointer to result (altered form of "val")

const char *jhcMorphFcns::noun_stem (char *val, UL32 tags) const
{
  int end = (int) strlen(val) - 1;

  // check for known forms
  if ((tags & JTAG_NPL) != 0)
  {
    // possibly remove final -s then check for s-es and i-es cases
    if ((end > 0) && (val[end] == 's'))
    {
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


//= Applies standard morphology to value in supplied verb in entry (altered!).
// pair input is part of an association list with a part of speech like "V-ED=washed"
// also sets "tags" to proper morphology bits as a mask (not value)
// returns canonical version of value (present) or NULL if not a verb
// NOTE: string might be a temporary array which can be rewritten (use soon)

const char *jhcMorphFcns::BaseVerb (UL32& tags, char *pair) const
{
  char *val = SlotRef(pair);
  const char *irr;

  // check for verbatim echo
  if (val == NULL)
    return NULL;
  if (SlotMatch(pair, "SAY"))
  {
    tags = JTAG_VIMP;
    return val;
  }

  // check for exception then try standard stemming
  tags = gram_tag(pair);
  if ((tags & JTAG_VERB) == 0)
    return NULL;
  if ((irr = get_base(val, tags)) != NULL)
      return irr;
  return verb_stem(val, tags);
}


//= Strip tense suffixes from surface word to get base verb.
// returns pointer to result (altered form of "val")

const char *jhcMorphFcns::verb_stem (char *val, UL32 tags) const
{
//  int n = (int) strlen(val), strip = 0;
//  char *end = val + n;

  int end = (int) strlen(val) - 1;

  // check for known forms
  if ((tags & JTAG_VPRES) != 0)
  {
    // active verb ending in -s
    if ((end > 0) && (val[end] == 's'))
      val[end] = '\0';
  }
  else if ((tags & JTAG_VPAST) != 0)
  {
    // past tense verb ending in -ed
    if ((end > 1) && (val[end - 1] == 'e') && (val[end] == 'd'))                 
    {
      // doubled consonant ("grabbed")
      if ((end > 3) && (val[end - 3] == val[end - 2]))
        val[end - 2] = '\0';
      else
        val[end - 1] = '\0';
    }
  }
  else if ((tags & JTAG_VPROG) != 0)
  {
    // progressive verb ending in -ing
    if ((end > 2) && (val[end - 2] == 'i') && (val[end - 1] == 'n') && (val[end] == 'g'))                 
    {
      // doubled consonant ("tipping")
      if ((end > 4) && (val[end - 4] == val[end - 3]))
        val[end - 3] = '\0';
      else
        val[end - 2] = '\0';
    }
  }
  return val;
}


//= Applies standard morphology to value in supplied adjective in entry (altered!).
// pair input is part of an association list with a part of speech like "ADJ-ER=bigger"
// also sets "tags" to proper morphology bits as a mask (not value)
// returns canonical version of value (present) or NULL if not an adjective
// NOTE: string might be a temporary array which can be rewritten (use soon)

const char *jhcMorphFcns::BaseAdj (UL32& tags, char *pair) const
{
  char *val = SlotRef(pair);
  const char *irr;

  tags = gram_tag(pair);
  if ((tags & JTAG_ADJ) == 0)
    return NULL;
  if ((irr = get_base(val, tags)) != NULL)
    return irr;
  return adj_stem(val, tags);
}


//= Strip tense suffixes from surface word to get base adjective.
// handles "bigger", "fuller", "noisier", and "nicer"
// returns pointer to result (altered form of "val")

const char *jhcMorphFcns::adj_stem (char *val, UL32 tags) const
{
  int n = (int) strlen(val), strip = 0;
  char *end = val + n;

  // check for known forms
  if ((tags & JTAG_COMP) != 0)
  {
    // comparative adjective ending in -er
    if ((n > 2) && (strcmp(end - 2 , "er") == 0))
      strip = 2;
  }
  else if ((tags & JTAG_SUP) != 0)
  {
    // superlative adjective ending in -est
    if ((n > 3) && (strcmp(end - 3, "est") == 0))
      strip = 3;
  }

  // remove ending found
  n -= strip;
  end = val + n;
  *end = '\0';

  // doubled consonant, transmuted y, and e elision
  if ((n > 2) && (*(end - 2) == *(end - 1)) && (*(end - 1) != 'l'))
    *(end - 1) = '\0';
  else if ((n > 1) && (*(end - 1) == 'i'))
    *(end - 1) = 'y';
  else if ((n > 2) && vowel(*(end - 2)) && !vowel(*(end - 1)))
  {
    *end = 'e';
    *(end + 1) = '\0';
  }
  return val;
}


//= Whether a particular character is a vowel.

bool jhcMorphFcns::vowel (char c) const
{
  return(strchr("aeiou", c) != NULL);
}


//= Convert parser class into a part of speech tag (mask not value).

UL32 jhcMorphFcns::gram_tag (const char *pair) const
{
  // noun counts
  if (SlotMatch(pair, "AKO"))          
    return JTAG_NSING;
  if (SlotMatch(pair, "AKO-S"))
    return JTAG_NPL;
  
  // verb tenses
  if (SlotMatch(pair, "ACT"))     
    return JTAG_VIMP;
  if (SlotMatch(pair, "ACT-S"))
    return JTAG_VPRES;
  if (SlotMatch(pair, "ACT-D"))
    return JTAG_VPAST;
  if (SlotMatch(pair, "ACT-G"))
    return JTAG_VPROG;

  // adjective forms
  if (SlotMatch(pair, "HQ"))          
    return JTAG_PROP;
  if (SlotMatch(pair, "HQ-ER"))          
    return JTAG_COMP;
  if (SlotMatch(pair, "HQ-EST"))
    return JTAG_SUP;

  // unknown
  return 0;
}


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= Retrieve the base form of a word given some possibly irregular surface form.
// needs specification of what POS mask applies to surface form
// returns special form if known, NULL if assumed regular 

const char *jhcMorphFcns::get_base (const char *surf, UL32 tags) const
{
  // nouns
  if ((tags & JTAG_NPL) != 0)
    return scan_for(surf, npl, nsing, nn);

  // verbs
  if ((tags & JTAG_VPRES) != 0)
    return scan_for(surf, vpres, vimp, nv);
  if ((tags & JTAG_VPROG) != 0)
    return scan_for(surf, vprog, vimp, nv);
  if ((tags & JTAG_VPAST) != 0)
    return scan_for(surf, vpast, vimp, nv);

  // adjectives 
  if ((tags & JTAG_COMP) != 0)
    return scan_for(surf, comp, adj, na);
  if ((tags & JTAG_SUP) != 0)
    return scan_for(surf, sup, adj, na);

  // invalid conversion
  return NULL;
}


//= Retrieve some possibly irregular surface form of a word given the base form.
// needs specification of what POS mask should be applied to base form
// returns special form if known, NULL if assumed regular 

const char *jhcMorphFcns::get_surface (const char *base, UL32 tags) 
{
  // nouns
  if ((tags & JTAG_NPL) != 0)
    return scan_for(base, nsing, npl, nn);

  // verbs
  if ((tags & JTAG_VPRES) != 0)
    return scan_for(base, vimp, vpres, nv);
  if ((tags & JTAG_VPROG) != 0)
    return scan_for(base, vimp, vprog, nv);
  if ((tags & JTAG_VPAST) != 0)
    return scan_for(base, vimp, vpast, nv);

  // adjectives 
  if ((tags & JTAG_COMP) != 0)
    return scan_for(base, adj, comp, na);
  if ((tags & JTAG_VPAST) != 0)
    return scan_for(base, adj, sup, na);

  // invalid conversion
  return NULL;
}


//= Look in list of keys for probe and get corresponding value.
// return NULL if probe not found or no special case listed

const char *jhcMorphFcns::scan_for (const char *probe, const char key[][40], const char val[][40], int n) const
{
  int i;

  for (i = 0; i < n; i++)
    if (strcmp(key[i], probe) == 0)
    {
      if (val[i][0] == '\0')
        return NULL;
      return val[i];
    }
  return NULL;
}


///////////////////////////////////////////////////////////////////////////
//                               Utilities                               //
///////////////////////////////////////////////////////////////////////////

//= Generate a derived lexicon grammar file from a base open-class grammar file.
// should check this by eye to find cases where incorrect forms are produced

int jhcMorphFcns::LexDeriv (const char *gram) const
{

  return 1;
}


//= Generate a list of base words from a derived lexicon file.
// should check to make sure no weird stemming problems occur

int jhcMorphFcns::LexBase (const char *deriv) const
{
  return 1;
}