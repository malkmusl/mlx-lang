// The translator's test of: see tools/check_lattice_emit.sh (jumps).
namespace jumps {
// Labels at the function's top, jumps forwards and back (a parser's loop).
static unsigned machine(const unsigned char *input, unsigned length)
{
    unsigned at = 0;
    unsigned state = 0;
    unsigned total = 0;
    unsigned stack[8];
    unsigned depth = 0;

next:
    if (at >= length) goto done;
    state = input[at++];
    goto dispatch;

reduce:
    if (depth == 0) goto error;
    total += stack[--depth] * 3;
    goto next;

dispatch:
    switch (state)
    {
        case 0:
            goto next;
        case 1:
        case 2:
            if (depth == 8) goto error;
            stack[depth++] = state + at;
            break;
        case 3:
            goto reduce;
        case 9:
            total += 1000;
            goto done;
        default:
            total += state;
            break;
    }
    goto next;

error:
    total = total * 2 + 7;

done:
    while (depth > 0) total += stack[--depth];
    return total;
}

// A switch whose cases run into the next, and a break inside an if.
static unsigned falls(unsigned kind, unsigned value)
{
    unsigned total = 0;
    switch (kind)
    {
        case 0:
            total += 1;
        case 1:
            total += 10;
            if (value > 5) break;
            total += 100;
        case 2:
        case 3:
            total += 1000;
            break;
        case 7 ... 9:
            total += 5;
        default:
            total += 10000;
    }
    return total + value;
}

// Labels in a loop's body (a scanner's loop), a continue of the loop from
// inside the labelled part, a break of the loop from inside a switch.
static unsigned scanner(const char *text)
{
    unsigned total = 0;
    unsigned position = 0;
    for (unsigned round = 0; round < 40; round++)
    {
        char ch = text[position];
        if (ch == 0) break;
        unsigned run = 0;
    match:
        if (text[position] == ch)
        {
            position++;
            run++;
            goto match;
        }
    action:
        switch (ch)
        {
            case 'a':
                total += run;
                continue;
            case 'b':
                if (run > 2)
                {
                    ch = 'a';
                    run = 100;
                    goto action;
                }
                total += run * 10;
                break;
            case 'c':
                total += 7;
                while (text[position] == 'x')
                {
                    position++;
                    if (text[position] == 'y') break;
                    if (text[position] == 'z') continue;
                    total += 1;
                }
                break;
            case '!':
                return total + 100000;
            default:
                total += 3;
        }
        total += 1000;
    }
    return total;
}

// Nested loops, a goto out of both.
static unsigned search(const unsigned *grid, unsigned width, unsigned height, unsigned wanted)
{
    unsigned found = 0;
    for (unsigned y = 0; y < height; y++)
    {
        for (unsigned x = 0; x < width; x++)
        {
            if (grid[y * width + x] == wanted)
            {
                found = y * 100 + x;
                goto out;
            }
        }
    }
    found = 9999;
out:
    return found;
}

unsigned run()
{
    const unsigned char program[] = {1, 2, 0, 3, 5, 3, 1, 3, 3, 9, 4};
    const unsigned char broken[] = {3, 1};
    const unsigned char full[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 6};
    unsigned total = machine(program, sizeof(program)) + machine(broken, sizeof(broken)) * 3 + machine(full, sizeof(full)) * 5;
    for (unsigned kind = 0; kind < 11; kind++) total = total * 3 + falls(kind, kind * 2 % 9);
    total = total * 7 + scanner("aaabbcxxxzxyqbbbbaac!zz") + scanner("ccxxxxbq");
    const unsigned grid[12] = {4, 8, 15, 16, 23, 42, 7, 9, 11, 13, 17, 19};
    total = total * 5 + search(grid, 4, 3, 42) + search(grid, 4, 3, 5) * 3 + search(grid, 3, 4, 17);
    return total;
}
}
