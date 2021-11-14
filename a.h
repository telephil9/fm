typedef struct Match Match;

struct Match
{
	char *name;
	int score;
};

int fuzzymatch(char *pat, char *s);
 