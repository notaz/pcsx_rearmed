/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#include "stdafx.h"
#include "externals.h"

// will be included from spu.c
#if 1 //def _IN_SPU

////////////////////////////////////////////////////////////////////////
// ADSR func
////////////////////////////////////////////////////////////////////////

INLINE void StartADSR(int ch)                          // MIX ADSR
{
  ADSRInfoEx *adsr = &spu.s_chan[ch].ADSRX;
  adsr->State = ADSR_ATTACK;                            // and init some adsr vars
  adsr->EnvelopeVol = 0;
  adsr->StepCounter = 0;
}

////////////////////////////////////////////////////////////////////////

enum ADSRtype {
  adsr_noexp,
  adsr_expinc,
  adsr_expdec,
};

INLINE forceinline int adsr_do_inner(int *samples, int ns, int ns_to,
    int *EnvelopeVol_, int env_step, int *cycles_, int cycles_step,
    int do_samples, enum ADSRtype type)
{
  int EnvelopeVol = *EnvelopeVol_;
  int cycles = *cycles_;
  int cycles_step_exp;
  switch (type)
  {
  case adsr_noexp:
    if (cycles_step == 0x8000)
    {
      for (; ns < ns_to && !(EnvelopeVol & 0x8000); ns++, EnvelopeVol += env_step)
        if (do_samples)
          samples[ns] = samples[ns] * EnvelopeVol >> 15;
    }
    else
    {
      while (ns < ns_to)
      {
        if (do_samples)
          samples[ns] = samples[ns] * EnvelopeVol >> 15;
        ns++;
        if ((cycles += cycles_step) & 0x8000)
        {
          cycles = 0;
          EnvelopeVol += env_step;
          if (EnvelopeVol & 0x8000)
            break;
        }
      }
    }
    break;

  case adsr_expinc:
    cycles_step_exp = cycles_step;
    if (EnvelopeVol >= 0x6000)
      cycles_step_exp = max(1, (cycles_step >> 2));
    while (ns < ns_to)
    {
      if (do_samples)
        samples[ns] = samples[ns] * EnvelopeVol >> 15;
      ns++;
      if ((cycles += cycles_step_exp) & 0x8000)
      {
        cycles = 0;
        cycles_step_exp = cycles_step;
        if (EnvelopeVol >= 0x6000)
          cycles_step_exp = max(1, (cycles_step >> 2));
        EnvelopeVol += env_step;
        if (EnvelopeVol >= 0x8000)
          break;
      }
    }
    break;

  case adsr_expdec:
    while (ns < ns_to)
    {
      if (do_samples)
        samples[ns] = samples[ns] * EnvelopeVol >> 15;
      ns++;
      if ((cycles += cycles_step) & 0x8000)
      {
        cycles = 0;
        EnvelopeVol += env_step * EnvelopeVol >> 15;
        if (EnvelopeVol <= 0)
          break;
      }
    }
  }
  *EnvelopeVol_ = EnvelopeVol;
  *cycles_ = cycles;
  return ns;
}

INLINE forceinline int adsr_do(int *samples, ADSRInfoEx *adsr, int ns_to,
    int do_samples)
{
  int EnvelopeVol = adsr->EnvelopeVol;
  int cycles_step, cycles = adsr->StepCounter;
  int ns = 0, level, env_step, rate;

  switch (adsr->State)
  {
  case ADSR_ATTACK:                                   // -> attack
    if (adsr->AttackRate == 0x7f)
      goto frozen;
    rate = adsr->AttackRate >> 2;
    cycles_step = max(1, 0x8000 >> max(0, rate - 11));
    env_step = (7 - (adsr->AttackRate & 3)) << max(0, 11 - rate);

    if (adsr->AttackModeExp)
      ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
             &cycles, cycles_step, do_samples, adsr_expinc);
    else
      ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
             &cycles, cycles_step, do_samples, adsr_noexp);
    if (EnvelopeVol < 0x7fff)
      break;
    EnvelopeVol = 0x7fff;
    adsr->State = ADSR_DECAY;
    // fallthrough to decay

  //--------------------------------------------------//
  case ADSR_DECAY:                                    // -> decay
    level = (adsr->SustainLevel + 1) << 11;
    cycles_step = 0x8000 >> max(0, adsr->DecayRate - 11);
    env_step = (uint32_t)-8 << max(0, 11 - (int)adsr->DecayRate);

    while (ns < ns_to)
    {
      if (do_samples)
        samples[ns] = samples[ns] * EnvelopeVol >> 15;
      ns++;
      if ((cycles += cycles_step) & 0x8000)
      {
        cycles = 0;
        EnvelopeVol += env_step * EnvelopeVol >> 15;
        if (EnvelopeVol < level)
          break;
      }
    }
    if (EnvelopeVol >= level)
      break;
    adsr->State = ADSR_SUSTAIN;
    adsr->EnvelopeVol = EnvelopeVol;
    // fallthrough to sustain

  //--------------------------------------------------//
  case ADSR_SUSTAIN:                                  // -> sustain
    if (adsr->SustainIncrease && EnvelopeVol >= 0x7fff)
    {
      EnvelopeVol = 0x7fff;
      ns = ns_to;
      break;
    }
    if (adsr->SustainRate == 0x7f)
      goto frozen;
    rate = adsr->SustainRate >> 2;
    cycles_step = max(1, 0x8000 >> max(0, rate - 11));
    env_step = (7 - (adsr->SustainRate & 3)) << max(0, 11 - rate);

    if (adsr->SustainIncrease)
    {
      if (adsr->SustainModeExp)
        ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
               &cycles, cycles_step, do_samples, adsr_expinc);
      else
        ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
               &cycles, cycles_step, do_samples, adsr_noexp);
      if (EnvelopeVol >= 0x7fff)
      {
        EnvelopeVol = 0x7fff;
        ns = ns_to;
      }
    }
    else
    {
      env_step = ~env_step;
      if (adsr->SustainModeExp)
        ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
               &cycles, cycles_step, do_samples, adsr_expdec);
      else
        ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
               &cycles, cycles_step, do_samples, adsr_noexp);
      if (EnvelopeVol <= 0)
      {
        EnvelopeVol = 0;
        if (do_samples)
          for (; ns < ns_to; ns++)
            samples[ns] = 0;
      }
    }
    break;

  //--------------------------------------------------//
  case ADSR_RELEASE:
    if (adsr->ReleaseRate == 0x1f)
      goto frozen;
    cycles_step = max(1, 0x8000 >> max(0, (int)adsr->ReleaseRate - 11));
    env_step = (uint32_t)-8 << max(0, 11 - (int)adsr->ReleaseRate);

    if (adsr->ReleaseModeExp)
      ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
             &cycles, cycles_step, do_samples, adsr_expdec);
    else
      ns = adsr_do_inner(samples, ns, ns_to, &EnvelopeVol, env_step,
             &cycles, cycles_step, do_samples, adsr_noexp);

    EnvelopeVol = max(0, EnvelopeVol);
    break;
  }

  adsr->EnvelopeVol = EnvelopeVol;
  adsr->StepCounter = cycles;
  return ns;

frozen:
  if (EnvelopeVol < 0x7ff0 && do_samples)
  {
    for (; ns < ns_to; ns++)
      samples[ns] = samples[ns] * EnvelopeVol >> 15;
  }
  return ns_to;
}

static int MixADSR(int *samples, ADSRInfoEx *adsr, int ns_to)
{
  return adsr_do(samples, adsr, ns_to, 1);
}

static int SkipADSR(ADSRInfoEx *adsr, int ns_to)
{
  return adsr_do(NULL, adsr, ns_to, 0);
}

#endif

// vim:shiftwidth=2:expandtab
