#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//DISCLAIMER, i wrote this some years ago after i haven't programmed in some
//years... The lexer seems buggy, but I don't want to fix it.

#define NAME_MAX 1024
// operators are in order of precedence
struct token {
	enum {VAR, CONST, ASSIGN, ADD, SUB, MUL, DIV, EXP, LBRACK, RBRACK, EOL, END} what;
	union {
		char name[NAME_MAX];
		double value;
	};
};

int
isop(int what)
{
	return ASSIGN <= what && what < LBRACK;
}

/********************************************************************************
* SYMBOL TABLE
* simple fixed size symbol table (coalesced hash table)
*/
#define SYMTAB_SIZE 2053

struct symtab {
	char name[NAME_MAX]; //stupid trick: if name == "", this entry is empty
	double value;
	struct symtab *next;
} symtab[SYMTAB_SIZE];

/* random hash function from the internet
 * (https://stackoverflow.com/questions/7666509/hash-function-for-string) */
unsigned long
hash(const char *str)
{
	unsigned long hash = 5381;
	int c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	return hash;
}

int
bucket_is_empty(const struct symtab *t)
{
	return t->name[0] == '\0';
}

void
enter(const char *name, double value)
{
	int h = hash(name) % SYMTAB_SIZE;

	if (bucket_is_empty(symtab + h)) {
		strncpy(symtab[h].name, name, NAME_MAX);
		symtab[h].value = value;
		symtab[h].next = NULL;
	}
	else {
		/* walk the chain, check if already entered name */
		struct symtab *p = symtab + h;
		while (strcmp(name, p->name)) {
			if (!p->next)
				break;
			p = p->next;
		}
		if (!strcmp(name, p->name)) { /* update existing bucket */
			p->value = value;
			return;
		}

		/* find an empty bucket */
		int i = SYMTAB_SIZE - 1;
		while (i >= 0) {
			if (bucket_is_empty(symtab + i))
				break;
			i--;
		}
		if (i < 0)
			errx(1, "symbol table was full when trying to enter <%s>\n", name);

		strncpy(symtab[i].name, name, NAME_MAX);
		symtab[i].value = value;
		symtab[i].next = NULL;

		/* point the last entry in the chain starting at symtab[h] to symtab[i] */
		p->next = &symtab[i];
	}
}

double
lookup(const char *name)
{
	unsigned long h = hash(name) % SYMTAB_SIZE;
	struct symtab *b = symtab + h;
	if (bucket_is_empty(b)) {
		errx(1, "variable <%s> doesn't exist\n", name);
	}
	while (b && strcmp(b->name, name))
		b = b->next;
	if (!b)
		errx(1, "variable <%s> doesn't exist\n", name);
	return b->value;
}
/********************************************************************************
 * LEXER
 * the spec says to ignore whitespace (i.e. "1 2" == "12"), but fuck that
 */
double
collect_num(void)
{
	double d;
	if (1 != scanf("%lf", &d))
		errx(1, "couldn't read a number!\n");
	return d;
}

void
collect_name(char *name)
{
	int c, i = 0;

	while (EOF != (c = getchar()) && isalnum(c)) {
		if (i < NAME_MAX - 1)
			name[i++] = c;
	}
	name[i] = 0;
	ungetc(c, stdin);
}

void
print_token(const struct token *t)
{
	switch (t->what) {
		case END:
			printf("end-of-file\n");
			break;
		case EOL:
			printf("end-of-line\n");
			break;
		case VAR:
			printf("variable: <%s>\n", t->name);
			break;
		case CONST:
			printf("constant: <%lf>\n", t->value);
			break;
		default:
			printf("operator: <%c>\n", "..=+-*/^()"[t->what]);
			break;
	}
}

/* get next token from stdin
 * if token is VAR or CONST, fill in val
 * allocate memory for name (leaky but meh) */
struct token
lex()
{
	struct token token = { 0 };
	static enum {OPERATOR, OPERAND} expect = OPERAND;
	int c = getchar();

	/* skip whitespace except newlines */
	while (isspace(c) && c != '\n')
		c = getchar();

	if (c == EOF) {
		token.what = END;
		expect = OPERAND;
	} else if (c == '\n') {
		token.what = EOL;
		expect = OPERAND;
	}
	else if (isdigit(c) || (expect == OPERAND && '-' == c)) {
		// negative numbers are the entire reason for 'expect' variable
		token.what = CONST;
		ungetc(c, stdin);
		token.value = collect_num();
		expect = OPERATOR;
	}
	else if (isalpha(c)) {
		token.what = VAR;
		ungetc(c, stdin);
		collect_name(token.name);
		expect = OPERATOR;
	}
	else {
		switch (c) {
			case '=':
				token.what = ASSIGN;
				break;
			case '+':
				token.what = ADD;
				break;
			case '-':
				token.what = SUB;
				break;
			case '*':
				token.what = MUL;
				break;
			case '/':
				token.what = DIV;
				break;
			case '^':
				token.what = EXP;
				break;
			case '(':
				token.what = LBRACK;
				break;
			case ')':
				token.what = RBRACK;
				break;
			default:
				errx(1, "bad input: %c.\n", c);
				break;
		}
		if (token.what != LBRACK) {
			expect = OPERAND;
		} else {
			expect = OPERATOR;
		}
	}

	return token;
}


/********************************************************************************
 * MAIN
 */
#define STK_MAX 256
struct token valuestack[STK_MAX];
struct token opstack[STK_MAX];
int value_idx = 0, op_idx = 0;

void
pushvalue(struct token t)
{
	if (value_idx + 1 == STK_MAX)
		errx(1, "value stack is full\n");
	
	valuestack[value_idx++] = t;
}

void
pushop(struct token t)
{
	if (op_idx + 1 == STK_MAX)
		errx(1, "operand stack is full\n");
	opstack[op_idx++] = t;
}

int
popop(void)
{
	if (op_idx-- == 0)
		errx(1, "operand stack is empty\n");
	return opstack[op_idx].what;
}

int
peekop(void)
{
	if (op_idx == 0)
		return 0;
	else return opstack[op_idx-1].what;
}

double
popval(void)
{
	if (value_idx-- == 0)
		errx(1, "value stack is empty\n");
	struct token *this = valuestack + value_idx;
	if (this->what == VAR)
		return lookup(this->name);
	else return this->value;
}

char *
poplval(void)
{
	if (value_idx-- == 0)
		errx(1, "value stack is empty\n");
	struct token *this = valuestack + value_idx;
	if (this->what != VAR)
		errx(1, "value is not assignable\n");
	return this->name;
}

void
exec(int op)
{
	double l,r;
	struct token result;
	result.what = CONST;
	switch (op) {
		case ASSIGN:
			result.value = popval();
			enter(poplval(), result.value);
			break;
		case ADD:
			result.value = popval() + popval();
			break;
		case SUB:
			r = popval();
			l = popval();
			result.value = l - r;
			break;
		case MUL:
			result.value = popval() * popval();
			break;
		case DIV:
			r = popval();
			l = popval();
			result.value = l / r;
			break;
		case EXP:
			r = popval();
			l = popval();
			result.value = pow(l,r);
			break;
	}
	pushvalue(result);
}

//XXX doesn't do associativity (left-right/right-left)
//i.e. 5 - 1 - 1 will be 5, not 3...
//it does do operator precedence
int
main(int argc, char **argv)
{
	struct token token;
	for (token = lex(); token.what != END; token = lex()) {
		if (token.what == EOL) {
			while (peekop())
				exec(popop());
			printf("%lf\n", popval());
			if (value_idx != 0 || op_idx != 0) {
				warn("bad expression: end of line but stack still full\n");
				value_idx = 0;
				op_idx = 0;
			}
		}
		else if (token.what == CONST || token.what == VAR) {
			pushvalue(token);
		}
		else if (token.what == LBRACK) {
			pushop(token);
		}
		else if (token.what == RBRACK) {
			int op;
			while (LBRACK != (op = popop()))
				exec(op);
		}
		else {
			/* token must be operator */
			while (isop(peekop()) && peekop() > token.what) {
				exec(popop());
			}
			pushop(token);
		}
	}
	return 0;
}
