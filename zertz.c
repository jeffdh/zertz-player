#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

typedef enum marble {
	NONE = 0,
	WHITE = 1,
	GREY = 2,
	BLACK = 3,
	MAX_MARBLE = 4,
} Marble;

typedef struct piece {
	bool exists;
	Marble marble;
} Piece;

typedef struct pair {
	int x, y;
} Pair;

#define BSIZE 7

typedef struct state {
	int player;
	Piece board[BSIZE * BSIZE];
	int score[2][MAX_MARBLE - 1];
} State;

#define boardxy(s, x, y) ((s)->board[(x) + (y) * BSIZE])
#define boardp(s, p) boardxy(s, (p).x, (p).y)

const Pair neighbors[] = {
	{ -1, -1 },
	{ -1, 0 },
	{ 0, 1 },
	{ 1, 1 },
	{ 1, 0 },
	{ 0, -1 }
};

#define nbor_n(p, n) { (p).x + neighbors[n].x, (p).y + neighbors[n].y }

bool validp(Pair p) {
	return p.x >= 0 && p.x < BSIZE && p.y >= 0 && p.y < BSIZE;
}

int score_value(State *s, int p) {
	int values[4] = {}, ret = 0;

	for (int i = 0; i < 3; i++) {
		values[i] = 3 + i - s->score[p][i];
		values[3] += max(0, 2 - s->score[p][i]);
	}

	for (int i = 0; i < 4; i++) {
		/*printf("values[%d] = %d/%d\n", i, values[i],
		    1 << ((6 - values[i]) << 1));*/
		ret += 1 << ((6 - values[i]) << 1);
	}

	return ret;
}

int heuristic(State *s, int p) {
	return score_value(s, p) - score_value(s, 1 - p);
}

bool is_free(State *s, Pair p) {
	if (boardp(s, p).marble)
		return false;

	for (int i = 0; i < 6; i++) {
		Pair n1 = nbor_n(p, i), n2 = nbor_n(p, (i + 1) % 6);

		if ((!validp(n1) || !boardp(s, n1).exists) &&
		    (!validp(n2) || !boardp(s, n2).exists))
			return true;
	}

	return false;
}

void build_board(State *s) {
	int width = BSIZE;
	for (int i = 3; i < BSIZE; i++) {
		for (int j = BSIZE - width; j < BSIZE; j++) {
			boardxy(s, i, j).exists = 1;
			boardxy(s, 6 - i, 6 - j).exists = 1;
		}
		width--;
	}
}

void print_func(State *s, void (*cell_print)(State *, Pair, void *, char *),
    void *data) {
	int extra = BSIZE;
	char cell[4] = "   ";

	for (int i = 0; i < BSIZE; i++) {
		for (int k = 0; k < extra; k++) printf("  ");
		extra--;

		for (int j = 0; j < BSIZE; j++) {
			if (!boardxy(s, i, j).exists)
				printf("   ");
			else {
				Pair p = { i , j };
				sprintf(cell, " %1d ", boardxy(s, i, j).marble);
				if (cell_print)
					(*cell_print)(s, p, data, cell);
				cell[3] = 0;
				printf("%3s", cell);
			}
			putchar(' ');
		}
		putchar('\n');
	}
}

void print_coord_inner(State *s, Pair p, void *data, char *out) {
	sprintf(out, "%1d,%1d", p.x, p.y);
}

void print_coord(State *s) {
	print_func(s, print_coord_inner, NULL);
}

int _div(int a, unsigned int b) {
	int res = a / b;
	if( a < 0 && a % b != 0 ) { res -= 1; }
	return res;
}

int _mod(int a, unsigned int b) {
	int res = a % b;
	if( res < 0 ) { res += a; }
	return res;
}

void print_rotate_inner(State *s, Pair p, void *data, char *out) {
	Pair fin = { p.y, p.x - p.y + 3 };

	sprintf(out, "%1d,%1d", fin.x, fin.y);
}

void print_rotate(State *s) {
	print_func(s, print_rotate_inner, NULL);
}


void print_select_inner(State *s, Pair p, void *data, char *out) {
	Pair *match = data;
	if (memcmp(&p, match, sizeof(p)))
		return;

	sprintf(out, "_%1d_", boardp(s, p).marble);
}

void print_select(State *s, int x, int y) {
	Pair p = { x, y };
	print_func(s, print_select_inner, &p);
}

void print_board(State *s) {
	print_func(s, NULL, NULL);
	printf("Score P1(%d): %d/%d/%d, P2(%d): %d/%d/%d\n",
	    heuristic(s, 0), s->score[0][0], s->score[0][1],
	    s->score[0][2], heuristic(s, 1), s->score[1][0],
	    s->score[1][1], s->score[1][2]);
}

#define MAX_JUMPS 25

typedef struct move_iter {
	bool is_jump;

	Pair curr;

	int jump_i[MAX_JUMPS];

	Pair adds[BSIZE * BSIZE];
	Pair removes[BSIZE * BSIZE];

	int adds_n;
	int removes_n;

	int adds_i;
	int removes_i;
	int colors_i;
} MoveIter;

typedef struct move {
	bool is_jump;

	Pair remove;
	Pair add;
	int marble;

	Pair jump_start;
	int jump[MAX_JUMPS];
	int jump_n;
} Move;

bool is_valid_jump(State *s, Pair start, Pair skip, Pair land) {
	return validp(skip) && boardp(s, skip).exists &&
	    boardp(s, skip).marble && validp(land) && boardp(s, land).exists &&
	    !boardp(s, land).marble;
}

bool apply_next_jump(State *s, MoveIter *iter, Pair start, int depth,
    Move *move) {

	State temp;
	int starting = iter->jump_i[depth];

	for (; iter->jump_i[depth] < 6; iter->jump_i[depth]++) {
		Pair skip = nbor_n(start, iter->jump_i[depth]);
		Pair land = nbor_n(skip, iter->jump_i[depth]);

		if (!is_valid_jump(s, start, skip, land))
			continue;

		memcpy(&temp, s, sizeof(temp));

		move->is_jump = iter->is_jump = true;
		move->jump[depth] = iter->jump_i[depth];

		boardp(&temp, land).marble = boardp(&temp, start).marble;
		boardp(&temp, start).marble = NONE;
		boardp(&temp, skip).marble = NONE;

		if (!apply_next_jump(&temp, iter, land, depth + 1, move)) {
			move->jump_n = depth + 1;
			iter->jump_i[depth]++;
		}

		if (starting == 0)
			return true;
	}

	iter->jump_i[depth] = 0;

	return false;
}

void find_next_valid(MoveIter *iter) {
	if (++iter->colors_i >= MAX_MARBLE) {
		iter->colors_i = 1;
		iter->removes_i++;
	}

	while (iter->adds_i < iter->adds_n) {
		if (iter->removes_i >= iter->removes_n) {
			iter->removes_i = 0;
			iter->adds_i++;
			continue;
		}

		if (iter->adds[iter->adds_i].x ==
		    iter->removes[iter->removes_i].x &&
		    iter->adds[iter->adds_i].y ==
		    iter->removes[iter->removes_i].y) {
			iter->removes_i++;
			continue;
		}

		break;
	}
}

bool apply_next_move(State *s, MoveIter *iter, Move *move) {
	memset(move, 0, sizeof(*move));
	for (; iter->curr.x < BSIZE; iter->curr.x++) {
		for (; iter->curr.y < BSIZE; iter->curr.y++) {
			if (!boardp(s, iter->curr).exists)
				continue;

			if (boardp(s, iter->curr).marble &&
			    apply_next_jump(s, iter, iter->curr, 0, move)) {
				move->jump_start = iter->curr;
				return true;
			}

			if (iter->is_jump)
				continue;

			if (!boardp(s, iter->curr).marble)
				iter->adds[iter->adds_n++] = iter->curr;
			if (is_free(s, iter->curr))
				iter->removes[iter->removes_n++] = iter->curr;
		}
		iter->curr.y = 0;
	}

	if (iter->is_jump || iter->adds_i >= iter->adds_n)
		return false;

	if (iter->colors_i == 0) {
		if (iter->adds_n > 30 && iter->removes_n > 10) {
			iter->adds_n /= 2;
			iter->removes_n /= 2;
		}
		find_next_valid(iter);
	}

	move->marble = iter->colors_i;
	move->add = iter->adds[iter->adds_i];
	move->remove = iter->removes[iter->removes_i];

	find_next_valid(iter);

	return true;
}

bool is_detached(State *s, Pair p) {
	if (!validp(p) || !boardp(s, p).exists)
		return true;

	if (!boardp(s, p).marble)
		return false;

	boardp(s, p).exists = 0;
	s->score[s->player][boardp(s, p).marble - 1]++;

	for (int i = 0; i < 6; i++) {
		Pair check = nbor_n(p, i);

		if (!is_detached(s, check))
			return false;
	}

	return true;
}

void do_detachments(State *s, Pair removed) {
	State temp;

	for (int i = 0; i < 6; i++) {
		Pair check = nbor_n(removed, i);

		memcpy(&temp, s, sizeof(temp));

		if (is_detached(&temp, check))
			is_detached(s, check);
	}
}

void do_move(State *s, Move *m) {
	if (m->is_jump) {
		Pair curr = m->jump_start;
		for (int i = 0; i < m->jump_n; i++) {
			Pair skip = nbor_n(curr, m->jump[i]);
			Pair land = nbor_n(skip, m->jump[i]);

			// Move jumped
			boardp(s, land).marble = boardp(s, curr).marble;
			boardp(s, curr).marble = NONE;

			// Count skipped
			s->score[s->player][boardp(s, skip).marble - 1]++;
			boardp(s, skip).marble = NONE;
			curr = land;
		}
	} else {
		boardp(s, m->add).marble = m->marble;
		boardp(s, m->remove).exists = 0;
		// check for disjoint set

		do_detachments(s, m->remove);
	}

	s->player = 1 - s->player;
}

void print_move(Move *m) {
	if (m->is_jump) {
		Pair p = m->jump_start;
		printf("jump(%d) %i,%i", m->jump_n, p.x, p.y);
		for (int i = 0; i < m->jump_n; i++) {
			Pair skip = nbor_n(p, m->jump[i]);
			Pair land = nbor_n(skip, m->jump[i]);

			printf(" -> %i,%i", land.x, land.y);
			p = land;
		}
		printf("\n");
	} else {
		printf("add=%i,%i remove=%i,%i marble=%i\n", m->add.x,
		    m->add.y, m->remove.x, m->remove.y, m->marble);
	}
}

void read_pair(State *s, Pair *pair_out) {
	Pair p;
	int n;
	for (;;) {
		fprintf(stderr, "x,y> ");
		n = scanf("%d %d", &p.x, &p.y);
		if (n != 2 || !validp(p)) {
			puts("failed to get valid pair");
			continue;
		} else if (!boardp(s, p).exists) {
			printf("%d,%d is gone\n", p.x, p.y);
			continue;
		}

		*pair_out = p;
		return;
	}
}

void read_empty_pair(State *s, Pair *pair_out) {
	for (;;) {
		read_pair(s, pair_out);
		if (boardp(s, *pair_out).marble) {
			puts("already has a marble, re-try\n");
			continue;
		}
		break;
	}
}

int read_color() {
	int n, c;
	for (;;) {
		fprintf(stderr, "color> ");
		n = scanf("%d", &c);
		if (n != 1 || c < 1 || c > 3) {
			puts("failed to get valid color\n");
			continue;
		}

		return c;
	}
}

bool is_win_for(State *s, int i) {
	return (s->score[i][0] == 3 || s->score[i][1] == 4 ||
	    s->score[i][2] == 5 || (s->score[i][0] == 2 &&
	    s->score[i][1] == 2 && s->score[i][2] == 2));
}

bool is_win(State *s) {
	for (int i = 0; i < 2; i++)
		if (is_win_for(s, i))
			return true;
	return false;
}

/*
Move *play_add_remove(State *s) {
	Pair add, remove;
	int c;

	printf("select board add\n");
	read_empty_pair(s, &add);
	print_select(s, add.x, add.y, 'a');

	c = read_color();
	printf("select board remove\n");

	for (;;) {
		read_pair(s, &remove);
		if (board_pair(s->board, remove).marble) {
			puts("has a marble, re-try\n");
			continue;
		} else if (add.x == remove.x && add.y == remove.y) {
			puts("can't remove where marble is going\n");
			continue;
		} else if (!is_free(s, remove)) {
			puts("not a free board piece\n");
			continue;
		}

		break;
	}

	Move *m = calloc(1, sizeof(Move));
	m->type = 0;
	m->remove = remove;
	m->add = add;
	m->marble = c;

	return m;
}
Move *play_jump(State *s, Move *possible) {
	int i = 1, n, k = 0;

	printf("select board jump\n");
	for (Move *curr = possible; curr; curr = curr->next) {
		printf("% 2d: ", i++);
		print_move(curr);
	}

	for (;;) {
		fprintf(stderr, "jump> ");
		n = scanf("%d", &k);
		if (k < 1 || k >= i) {
			puts("No such jump\n");
			continue;
		}
		break;
	}

	i = 1;
	for (Move *curr = possible; curr ; curr = curr->next) {
		if (i++ == k) {
			Move *m = malloc(sizeof(Move));
			memcpy(m, curr, sizeof(Move));
			m->next = NULL;
			return m;
		}
	}

	return NULL;
}
*/

int minimax(State *s, int max_player, int alpha, int beta, int depth,
    int print, int *count) {
	int h = heuristic(s, max_player);

	if (++(*count) % 10000000 == 0 || h != 0)
		printf("Iteration %d / %d\n", *count, h);

	if (depth == 0 || is_win(s))
		return h;

	int save, sub;
	save = sub = (s->player == max_player ? INT_MIN : INT_MAX);

	State temp;
	MoveIter iter = {};
	Move move, max_play = {};
	while (apply_next_move(s, &iter, &move)) {
		memcpy(&temp, s, sizeof(temp));
		do_move(&temp, &move);

		sub = minimax(s, max_player, alpha, beta, depth - 1, 0, count);
		if (s->player == max_player) {
			alpha = max(alpha, sub);
			if (alpha != save) {
				save = alpha;
				memcpy(&max_play, &move, sizeof(move));
			}
			if (beta <= alpha)
				break;
		} else {
			beta = min(beta, sub);
			if (beta <= alpha)
				break;
		}
	}

	if (print == 1) {
		printf("Best move (%d) is ", save);
		print_move(&max_play);
	}

	return (s->player == max_player ? alpha : beta);
}

int main() {
	State real_state = {};
	State *s = &real_state;

	build_board(s);
	print_coord(s);
	print_rotate(s);

	/*boardxy(s, 3, 3).marble = 1;
	boardxy(s, 4, 3).marble = 2;
	boardxy(s, 2, 4).marble = 2;
	boardxy(s, 3, 5).marble = 3;*/

	/*boardxy(s, 0, 0).marble = 1;
	boardxy(s, 1, 0).marble = 1;
	boardxy(s, 0, 1).exists = 0;
	boardxy(s, 0, 2).exists = 0;
	boardxy(s, 1, 2).exists = 0;
	boardxy(s, 2, 0).exists = 0;
	boardxy(s, 2, 1).exists = 0;
	boardxy(s, 3, 0).exists = 0;
	boardxy(s, 3, 1).exists = 0;
	boardxy(s, 3, 2).exists = 0;*/

	for (;;) {
		print_board(s);
		if (is_win(s))
			printf("Winner!\n");
		printf("Player %d turn\n", s->player + 1);

		for (int i = 1; i < 4; i++) {
			int count = 0;
			printf("Iterating at %d ", i);
			minimax(s, s->player, INT_MIN, INT_MAX, i, 1, &count);
			printf("Took %d iterations\n", count);
		}

		/*for (;;) {
			Move move = {};
			State new_state;
			memcpy(&new_state, s, sizeof(new_state));
			if (!apply_next_move(s, &iter, &move)) {
				//printf("No more moves (%d)\n", plays);
				break;
			}
			plays++;

			do_move(&new_state, &move);
			if (move.remove.x == 2 && move.remove.y == 2) {
				print_move(&move);
				print_board(&new_state);
			}
			n = heuristic(&new_state, 0);
			if (k < n) {
				memcpy(&best, &move, sizeof(best));
				k = n;
			}
			//print_board(&new_state);
		}
		print_move(&best);
		do_move(s, &best);
		print_board(s);
		*/
		break;
	}

	return 0;
}
