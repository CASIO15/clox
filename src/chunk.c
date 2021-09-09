#include <stdio.h>
#include "chunk.h"
#include "memory.h"

void initChunk(Chunk *chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->lines = NULL;
    // Initializing the constants struct
    initValueArray(&chunk->constants);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count++] = byte;

    compress(chunk, line);
}

void compress(Chunk* chunk, int line)
{
    // Allocation space for the lines struct
    if (chunk->lineCapacity < chunk->lineCount + 1) {
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines = GROW_ARRAY(lineTrace, chunk->lines, oldCapacity, chunk->lineCapacity);
    }

    if (chunk->lineCount == line - 1) {
        chunk->lines[chunk->lineCount].line = line;
        chunk->lines[chunk->lineCount].offset = chunk->count;
    } else {
        chunk->lineCount++;
        chunk->lines[chunk->lineCount].line = line;
        chunk->lines[chunk->lineCount].offset = chunk->count;
    }
}

void freeChunk(Chunk *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(lineTrace , chunk->lines, chunk->lineCapacity);
    // Freeing the constants struct
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value)
{
    writeValueArray(&chunk->constants, value);
    // Returns the index of the instruction
    return chunk->constants.count - 1;
}

int getLine(Chunk* chunk, int offset)
{
    bool flag = true;
    int i;

    for (i = 0; i < chunk->lineCount+1 && flag; i++) {
        if (offset < chunk->lines[i].offset)
            flag = false;
    }
    return chunk->lines[i-1].line;
}

void writeConstant(Chunk* chunk, Value value, int line)
{
    // Adding the value to the constants array
    int index = addConstant(chunk, value);

    // If the index value is less than 256 (int 8 bits range) no special work is done
    if (index < 256) {
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, (uint8_t) index, line);
    } else {
        // If the index is larger than 256, we store it using little-endian representation.
        // We are performing & on the lower 8 bits to get the value, and store in the line.
        writeChunk(chunk, OP_CONSTANT_LONG, line);
        writeChunk(chunk, (uint8_t) (index & 0xff), line);
        writeChunk(chunk, (uint8_t) ((index >> 8) & 0xff), line);
        writeChunk(chunk, (uint8_t) ((index >> 16) & 0xff), line);
    }
}

