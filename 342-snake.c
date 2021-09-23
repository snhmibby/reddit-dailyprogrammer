#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Snake on a hypercube problem.  A hypercube is a multidimensional cube...
 * i.e. a 1 dimensional hypercube is a line, a 2 dimensional is a rectangle, 3
 * dimensional a cube, a 4 dimensional would be 2 cubes, each corner of one
 * cube connected to its mirror in the other, and so on.  naive algorithm:
 * generate all paths, memorize the longest.
 *
 * A 'snake' is a path on this hypercube/graph where each node has a minimum
 * distance of 2 edges to nodes in the graph that are not its direct neigbours
 * in the path.
 */


#define NUMCORNERS (1 << dimensions)
int dimensions;
int *mark;
int *current_path, *longest_path;
int longest_path_length = -1;

//C = corner #,  N = neighbour, I = scratch
#define LOOP_NEIGHBOURS(C, N, I) \
	for (I = 0, N = C ^ 1; I < dimensions; ++I, N = C ^ (1 << I))

//simple brute force
void
visit(int corner, int pathlen)
{
	int i, n;

	LOOP_NEIGHBOURS(corner, n, i) {
		mark[n]++;
	}
	current_path[pathlen] = corner;

	/* count number of unmarked neighbours
	 *an "unmarked" neighbour has a mark of 1 at this point 
	 */
	int num_unmarked = 0;

	LOOP_NEIGHBOURS(corner, n, i) {
		if (1 == mark[n]) {
			num_unmarked++;
			visit(n, pathlen+1);
		}
	}

	/* if at the end of a path (no unmarked neighbours)
	 * check if longest path
	 */
	if (0 == num_unmarked && pathlen >= longest_path_length) {
		longest_path_length = 1 + pathlen;
		memcpy(longest_path, current_path, longest_path_length * sizeof(int));
	}
	
	LOOP_NEIGHBOURS(corner, n, i) {
		mark[n]--;
	}
}

void
print_corner(int x)
{
	int i;
	for (i = 0; i < dimensions; ++i) {
		putchar(x & (1 << i) ? '1': '0');
	}
}

int
main(void)
{
	int i;

	if (1 != scanf("%d", &dimensions)) {
		errx(1, "could not read # dimensions\n");
	}
	mark = malloc(NUMCORNERS * sizeof *mark);
	current_path = malloc(NUMCORNERS * sizeof *current_path);
	longest_path = malloc(NUMCORNERS * sizeof *longest_path);

	for (i = 0; i < NUMCORNERS; ++i) {
		mark[i] = 0;
	}

    mark[0]++;
	visit(0, 0);

	printf("%d = ", longest_path_length - 1); //print #edges on the path not corners
	print_corner(0);
	for (i = 1; i < longest_path_length; ++i) {
		printf(" -> ");
		print_corner(longest_path[i]);
	}

	return 0;
}
