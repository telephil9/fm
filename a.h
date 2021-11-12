typedef struct Match Match;

struct Match
{
	char *n;
	int s;
};

int fuzzymatch(char *pat, char *s);
 