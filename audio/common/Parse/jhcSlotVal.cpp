// jhcSlotVal.cpp : functions for manipulating association lists from parser
//
// Written by Jonathan H. Connell, jconnell@alum.mit.edu
//
///////////////////////////////////////////////////////////////////////////
//
// Copyright 2015-2020 IBM Corporation
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
#include <ctype.h>

#include "Parse/jhcSlotVal.h"


///////////////////////////////////////////////////////////////////////////
//                              Main Functions                           //
///////////////////////////////////////////////////////////////////////////

//= General conditional debugging message removes tabs from alist.
// mostly used by jhcNetBuild

void jhcSlotVal::CallList (int lvl, const char *fcn, const char *alist, const char *entry) const
{
  if (dbg < lvl)
    return;
  if (entry == NULL)
    jprintf("%s\n  ", fcn);
  else
    jprintf("%s [%s]\n  ", fcn, entry);
  PrintList(alist);
  jprintf("\n");
}


//= Print a shortened "pretty" version of association list (no tabs).

void jhcSlotVal::PrintList (const char *alist, const char *tag) const
{
  char out[500];
  int i, n0 = (int) strlen(alist), n = __min(n0, 499);

  // copy list with substitutions for some characters
  for (i = 0; i < n; i++)  
    if (alist[i] == ' ')
      out[i] = '_';
    else if (alist[i] == '\t')
      out[i] = ' ';
    else
      out[i] = alist[i];
  out[i] = '\0';

  // print new string, possibly with a prefix
  if (tag == NULL)
    jprintf("%s\n", out + 1);
  else
    jprintf("%s %s\n", tag, out + 1); 
}


//= Take a "pretty" version of an association list and convert to tab form.
// returns converted list

const char *jhcSlotVal::SetList (char *alist, const char *src, int ssz) const
{
  int i, n0 = (int) strlen(src), n = __min(n0, ssz - 2);

  if (n > 0)
    alist[0] = '\t';
  for (i = 0; i < n; i++)  
    if (src[i] == '_')
      alist[i + 1] = ' ';
    else if (src[i] == ' ')
      alist[i + 1] = '\t';
    else
      alist[i + 1] = src[i];
  alist[i + 1] = '\0';
  return alist;
}


//= Goes down list looking for any attentional marker.
// returns 1 if found, 0 if missing

int jhcSlotVal::ChkAttn (const char *alist) const
{
  if (FindSlot(alist, "ATTN", NULL, 0, 0) != NULL)
    return 1;
  return 0;
}


//= Alters given string by stripping prefixes like "r-" and removing internal dashes.
// returns pointer into original string (following chars might be changed)
// Example: "!r-foo-bar" --> "foo bar"

char *jhcSlotVal::CleanVal (char *dest) const
{
  char *d, *s = dest;

  // sanity check
  if (dest == NULL)
    return NULL;

  // strip fragment symbol and "r-" prefix
  if (strchr("!$%", *s) != NULL)
    s++;
  if ((s[0] != '\0') && (s[1] == '-'))
    s += 2;

  // remove any hyphens by changing characters
  d = s;
  while (*d != '\0')
  {
    if (*d == '-')
      *d = ' ';
    d++;
  }
  return s;
}


//= Forms new string by stripping prefixes like "r-" and removing internal dashes.
// Example: "!r-foo-bar" --> "foo bar"

char *jhcSlotVal::CleanVal (char *dest, const char *src) const
{
  char *d = dest;
  const char *s = src;           

  // sanity check
  if (dest == NULL)
    return NULL;

  if (src != NULL)
  {
    // strip fragment symbol and "r-" prefix
    if (strchr("!$%", *s) != NULL)
      s++;
    if ((s[0] != '\0') && (s[1] == '-'))
      s += 2;

    // copy rest but remove any hyphens
    while (*s != '\0')
    {
      *d++ = ((*s == '-') ? ' ' : *s);
      s++;
    }
  }

  // terminate copied string
  *d = '\0';
  return dest;
}


//= Advance to next entry of any type (slot-value pair or fragment) in alist.
// returns pointer to part of list after returned entry (NULL if none)

const char *jhcSlotVal::NextEntry (const char *alist, char *entry, int ssz) const
{
  const char *tail, *head = alist;

  // searches for tab separated entry (even first has a tab)
  if (head == NULL)
    return NULL;
  while (1)
  {
    if ((head = strchr(head, '\t')) == NULL)
      return NULL;
    head++;
    break;
  }

  // find end of entry, walking backwards over trailing whitespace
  if ((tail = strchr(head, '\t')) == NULL)
    tail = head + strlen(head);
  while (--tail > head)
    if (*tail != ' ')
      break; 
  tail++;

  // copy entry as a single string
  if (entry != NULL)
    strncpy_s(entry, ssz, head, tail - head);      // always adds terminator
  return tail;
}


//= Advance to next entry of any type in alist and compare with given tag.
// can restrict match to first n characters if n > 0
// returns pointer to part of list after returned entry (NULL if bad match)

const char *jhcSlotVal::NextMatches (const char *alist, const char *tag, int n) const
{
  char entry[200];
  const char *tail;
  int order;

  if ((tail = NextEntry(alist, entry)) == NULL)
    return NULL;
  if (n > 0)
    order = strncmp(entry, tag, n);
  else 
    order = strcmp(entry, tag);
  return((order == 0) ? tail : NULL);
}


///////////////////////////////////////////////////////////////////////////
//                              Slot Functions                           //
///////////////////////////////////////////////////////////////////////////

//= See if the current fragment has a tag of the single given type.

bool jhcSlotVal::HasSlot (const char *alist, const char *slot, int local) const
{
  if (FindSlot(alist, slot, NULL, local, 0) != NULL)
    return true;
  return false;
}


//= See if the current fragment has a tag of any of the given types.
// types separated by single spaces in probe list

bool jhcSlotVal::AnySlot (const char *alist, const char *marks, int local) const
{
  char slot[80];
  const char *tail, *head = marks;

  while (1)
  {
    // get next term in list
    tail = strchr(head, ' ');
    if (tail != NULL)
      strncpy_s(slot, head, tail - head);                // always adds terminator
    else
      strcpy_s(slot, head);

    // see if found in list of tags
    if (FindSlot(alist, slot, NULL, local, 0) != NULL)
      return true;
    if (tail == NULL)
      break;
    head = tail + 1;
  }
  return false;
}


//= Look for tag "slot" within the association list and bind its value.
// if local > 0 then only searches up until the next fragment marker
// does not change input "val" if slot is not found (for defaults)
// returns portion of list after entry for convenience, NULL if not found

const char *jhcSlotVal::FindSlot (const char *alist, const char *slot, char *val, int local, int ssz) const
{
  char s[80], v[200];
  const char *tail = alist;

  if ((slot == NULL) || (*slot == '\0'))
    return NULL;

  while ((tail = NextSlot(tail, s, v, local, 80, 200)) != NULL)
    if (_stricmp(s, slot) == 0)
    {
      if (val != NULL)
        strcpy_s(val, ssz, v);
      return tail;
    }
  return NULL;
}


//= Find the next slot value pair within the current fragment.
// if local > 0 then only searches up until the next fragment marker
// binds both the slot name and the associated value (unchanged if none)
// returns pointer to part of list after this pair (NULL if none)

const char *jhcSlotVal::NextSlot (const char *alist, char *slot, char *val, int local, int ssz, int vsz) const
{
  char entry[200];
  const char *sep, *tail = alist;

  // check input
  if ((alist == NULL) || (*alist == '\0'))
    return NULL;

  while (1)
  {
    // possibly stop if new fragment or run out of list
    if ((tail = NextEntry(tail, entry, 200)) == NULL)
      return NULL;
    if ((local > 0) && (strchr("!$%", *entry) != NULL))
      return NULL;

    // check format and find end of slot string 
    if ((sep = strchr(entry, '=')) != NULL)
      break;
  }

  // copy out slot name
  if (slot != NULL)
    strncpy_s(slot, ssz, entry, sep - entry);      // always adds terminator

  // copy value string
  if (val != NULL)
    strcpy_s(val, vsz, sep + 1);
  return tail;
}


//= See if slot-value pair has exactly the given slot.

bool jhcSlotVal::SlotMatch (const char *pair, const char *slot) const
{
  int n = SlotStart(pair, slot);

  return((n > 0) && (pair[n] == '='));
}


//= See if slot-value pair begins with the given prefix (if any).
// returns length of prefix if matched, negative otherwise

int jhcSlotVal::SlotStart (const char *pair, const char *prefix) const
{
  int n;

  if (prefix == NULL)
    return 0;
  n = (int) strlen(prefix);
  if (strncmp(pair, prefix, n) != 0)
    return -1;
  return n;
}


//= Simple parsing of slot-value pair to return value part.
// return pointer allows "pair" itself to be altered (!)

char *jhcSlotVal::SlotRef (char *pair) const
{
  char *val;

  if ((val = strchr(pair, '=')) == NULL)
    return NULL;
  return(val + 1);
}


//= Extract the value a from pair if its slot name begins with the given prefix (if any).
// returns following value pointer into pair string (NULL if prefix does not match)

const char *jhcSlotVal::SlotGet (char *pair, const char *prefix, int lower) const
{
  char *sep;
  int n;

  if ((n = SlotStart(pair, prefix)) < 0)
    return NULL;
  if ((sep = strchr(pair + n, '=')) == NULL)
    return NULL;
  if (lower > 0)
    all_lower(sep + 1);
  return(sep + 1);
}


//= Convert a word (in place) to all lowercase characters.
// returns length of word for convenience

int jhcSlotVal::all_lower (char *txt) const
{
  int i, n = (int) strlen(txt);

  for (i = 0; i < n; i++)
    txt[i] = (char) tolower(txt[i]);
  return n;
}


///////////////////////////////////////////////////////////////////////////
//                           Fragment Functions                          //
///////////////////////////////////////////////////////////////////////////

//= See if the association list has a fragment of the single given kind.
// returns 1 if present, 0 if missing

bool jhcSlotVal::HasFrag (const char *alist, const char *frag) const
{
  if (FindFrag(alist, frag) != NULL)
    return true;
  return false;
}


//= See if the association list has a fragment of any of the given kinds.
// kinds separated by single spaces in probe list 
// returns 1 if present, 0 if missing

bool jhcSlotVal::AnyFrag (const char *alist, const char *kinds) const
{
  char frag[200];
  const char *tail, *head = kinds;

  while (1)
  {
    // get next term in list
    tail = strchr(head, ' ');
    if (tail != NULL)
      strncpy_s(frag, head, tail - head);        // always adds terminator
    else
      strcpy_s(frag, head);

    // see if found in list of tags
    if (FindFrag(alist, frag) != NULL)
      return true;
    if (tail == NULL)
      break;
    head = tail + 1;
  }
  return false;
}


//= Look through association list to find fragment of given type.
// returns portion of list after entry for convenience, NULL if not found

const char *jhcSlotVal::FindFrag (const char *alist, const char *frag) const
{
  char kind[200];
  const char *tail = alist;

  while ((tail = NextFrag(tail, kind, 200)) != NULL)
    if (_stricmp(kind, frag) == 0)
      return tail;
  return NULL;
}


//= Advance to next fragment and bind type.
// returns pointer to part of list after fragment marker

const char *jhcSlotVal::NextFrag (const char *alist, char *frag, int ssz) const
{
  char entry[200];
  const char *tail = alist;

  while ((tail = NextEntry(tail, entry, 200)) != NULL)
    if (strchr("!$%", *entry) != NULL)
    {
      if (frag != NULL)
        strcpy_s(frag, ssz, entry);
      return tail;
    }
  return NULL;
}


//= Advance to next fragment within current fragment and bind type.
// returns pointer to part of list after COMPLETE fragment found

const char *jhcSlotVal::FragNextFrag (const char *alist, char *frag, int ssz) const
{
  char entry[200];
  const char *tail = alist;

  while ((tail = NextEntry(tail, entry, 200)) != NULL)
    if (strchr("!$%", *entry) != NULL)
    {
      if (entry[1] == '\0')            // end of main fragment encountered
        return NULL;
      if (frag != NULL)
        strcpy_s(frag, ssz, entry);
      return FragClose(tail, 0);       // just past end of embedded fragment
    }
  return NULL;
}


//= Find and copy out next slot-value pair within this same fragment.
// stays within current fragment, skipping over intervening embedded fragments
// returns pointer to remaining alist (NULL if not found)

const char *jhcSlotVal::FragNextPair (const char *alist, char *pair, int ssz) const
{
  const char *tail = alist;
  int depth = 0;

  // keep track of fragment nesting depth
  while ((tail = NextEntry(tail, pair, ssz)) != NULL)
    if (strchr("!$%", *pair) != NULL)          
    {
      depth += ((pair[1] == '\0') ? -1 : 1);   // embedded fragment  
      if (depth < 0)
        return NULL;
    }
    else if ((depth == 0) && (strchr(pair, '=') != NULL))
      return tail;
  return NULL;
}


//= See if fragment has given slot as part of top level structure.
// stays within current fragment, skipping over intervening embedded fragments
// note that HasSlot function will quit at start of first sub-fragment

bool jhcSlotVal::FragHasSlot (const char *alist, const char *slot) const
{
  char pair[200];
  char *sep;
  const char *tail = alist;

  while ((tail = FragNextPair(tail, pair)) != NULL)
  {
    sep = strchr(pair, '=');
    *sep = '\0';
    if (strcmp(pair, slot) == 0)
       return true;
  }
  return false;
}


//= Look for end of current fragment after possibly skipping fragment head.
// returns pointer to remaining alist (NULL if not found)

const char *jhcSlotVal::FragClose (const char *alist, int skip) const
{
  char entry[200];
  const char *tail = alist;
  int depth = ((skip > 0) ? -1 : 0);

  while ((tail = NextEntry(tail, entry)) != NULL)
    if (strchr("!$%", *entry) == NULL)
      continue;
    else if (entry[1] != '\0')         // embedded fragment
      depth++;
    else if (depth == 0)               // matched ending
      return tail;
    else                               // end of embedded
      depth--;
  return NULL;
}


