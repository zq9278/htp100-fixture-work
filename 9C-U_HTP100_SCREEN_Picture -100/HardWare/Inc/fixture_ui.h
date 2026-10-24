#ifndef FIXTURE_UI_H
#define FIXTURE_UI_H
#include <stdint.h>
#include <stddef.h>
void FixtureUi_Init(void);
void FixtureUi_Feed(const uint8_t *data,size_t size);
void FixtureUi_Tick(void);
#endif
