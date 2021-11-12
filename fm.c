#include <u.h>
#include <libc.h>
#include <ctype.h>
#include "a.h"

/* code ported from https://github.com/forrestthewoods/lib_fts */

enum
{
	Reclimit = 10,

	/* score modifiers */
	Bseq = 15,
	Bsep = 30,
	Bcamel = 30,
	Bfirst = 15,
	Plead = -5,
	Pmaxlead = -15,
	Punmatched = -1,
};

int
fuzzymatchrec(char *pat, char *s, char *ss, 
			  u8int *smatches, u8int *matches, int maxmatches, int nextmatch, 
			  int *reccount, int reclimit)
{
	int score, penalty, i, curridx, previdx, recmatch, bestrecscore, recscore, firstmatch, matched, unmatched;
	char neighbor, curr;
	u8int bestrecmatches[256], recmatches[256];

	*reccount += 1;
	if(*reccount >= reclimit)
		return -1;

	if(*pat == '\0' || *s == '\0')
		return -1;

	recmatch = 0;
	bestrecscore = 0;
	firstmatch = 1;
	while(*pat != '\0' && *s != '\0'){
		if(tolower(*pat) == tolower(*s)){
			if(nextmatch > maxmatches)
				return -1;
			if(firstmatch && smatches){
				memcpy(matches, smatches, nextmatch);
				firstmatch = 0;
			}
			recscore = fuzzymatchrec(pat, s + 1, ss, matches, recmatches, sizeof(recmatches), nextmatch, reccount, reclimit);
			if(recscore >= 0){
				if(!recmatch || recscore > bestrecscore){
					memcpy(bestrecmatches, recmatches, 256);
					bestrecscore = recscore;
				}
				recmatch = 1;
			}
			matches[nextmatch++] = (u8int)(s - ss);
			++pat;
		}
		++s;
	}

	score = -1;
	matched = *pat == '\0';
	if(matched){
		while(*s != '\0')
			++s;
		score = 100;
		penalty = Plead * matches[0];
		if(penalty < Pmaxlead)
			penalty = Pmaxlead;
		score += penalty;
		unmatched = (int)(s - ss) - nextmatch;
		score += Punmatched * unmatched;
		for(i = 0; i < nextmatch; i++){
			curridx = matches[i];
			if(i > 0){
				previdx = matches[i - 1];
				if(curridx == (previdx + 1))
					score += Bseq;
			}
			if(curridx > 0){
				neighbor = ss[curridx - 1];
				curr = ss[curridx];
				if(islower(neighbor) && isupper(curr))
					score += Bcamel;
				if(neighbor == '_' || neighbor == ' ')
					score += Bsep;
			}else
				score += Bfirst;
		}
	}

	if(recmatch && (!matched || bestrecscore > score)){
		memcpy(matches, bestrecmatches, maxmatches);
		return bestrecscore;
	}else if(matched)
		return score;
		
	return -1;
}

int
fuzzymatch(char *pat, char *s)
{
	u8int m[256];
	int r;

	r = 0;
	return fuzzymatchrec(pat, s, s, nil, m, sizeof(m), 0, &r, Reclimit);
}
