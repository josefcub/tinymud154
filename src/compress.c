/* Compression routines */

/* These use a pathetically simple encoding that takes advantage of the */
/* eighth bit on a char; if you are using an international character set, */
/* they may need substantial patching. */

#ifdef COMPRESS

#define BUFFER_LEN 16384	/* nice big buffer */

#define TOKEN_BIT 0x80		/* if on, it's a token */
#define TOKEN_MASK 0x7f		/* for stripping out token value */
#define NUM_TOKENS (128)
#define MAX_CHAR (128)

/* Top 128 bigrams in the CMU TinyMUD database as of 2/13/90 */ 
static const char *tokens[NUM_TOKENS] = {
    "e ", " t", "th", "he", "s ", " a", "ou", "in",
    "t ", " s", "er", "d ", "re", "an", "n ", " i",
    " o", "es", "st", "to", "or", "nd", "o ", "ar",
    "r ", ", ", "on", " b", "ea", "it", "u ", " w",
    "ng", "le", "is", "te", "en", "at", " c", "y ",
    "ro", " f", "oo", "al", ". ", "a ", " d", "ut",
    " h", "se", "nt", "ll", "g ", "yo", " l", " y",
    " p", "ve", "f ", "as", "om", "of", "ha", "ed",
    "h ", "hi", " r", "lo", "Yo", " m", "ne", "l ",
    "li", "de", "el", "ta", "wa", "ri", "ee", "ti",
    "no", "do", "Th", " e", "ck", "ur", "ow", "la",
    "ac", "et", "me", "il", " g", "ra", "co", "ch",
    "ma", "un", "so", "rt", "ai", "ce", "ic", "be",
    " n", "k ", "ge", "ot", "si", "pe", "tr", "wi",
    "e.", "ca", "rs", "ly", "ad", "we", "bo", "ho",
    "ir", "fo", "ke", "us", "m ", " T", "di", ".." };

static char token_table[MAX_CHAR][MAX_CHAR];
static int table_initialized = 0;

static void init_compress(void)
{
    int i;
    int j;

    for(i = 0; i < MAX_CHAR; i++) {
	for(j = 0; j < MAX_CHAR; j++) {
	    token_table[i][j] = 0;
	}
    }

    for(i = 0; i < NUM_TOKENS; i++) {
	token_table[tokens[i][0]][tokens[i][1]] = i | TOKEN_BIT;
    }

    table_initialized = 1;
}

static int compressed(const char *s)
{
    while(*s) {
	if(*s++ & TOKEN_BIT) return 1;
    }
    return 0;
}

const char *compress(const char *s)
{
    static char buf[BUFFER_LEN];
    char *to;
    char token;

    if(!table_initialized) init_compress();

    if(compressed(s)) return s;	/* already compressed */

    /* tokenize the first characters */
    for(to = buf; s[0] && s[1]; to++) {
	if(token = token_table[s[0]][s[1]]) {
	    *to = token;
	    s += 2;
	} else {
	    *to = s[0];
	    s++;
	}
    }

    /* copy the last character (if any) and null */
    while(*to++ = *s++);

    return buf;
}
    
const char *uncompress(const char *s)
{
    static char buf[BUFFER_LEN];
    char *to;
    const char *token;

    for(to = buf; *s; s++) {
	if(*s & TOKEN_BIT) {
	    token = tokens[*s & TOKEN_MASK];
	    *to++ = *token++;
	    *to++ = *token;
	} else {
	    *to++ = *s;
	}
    }

    *to++ = *s;

    return buf;
}

#endif
	    
