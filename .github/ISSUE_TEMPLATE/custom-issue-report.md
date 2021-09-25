---
name: custom-issue-report
about: A brief template for bug/issue reports
title: ''
labels: ''
assignees: ''

---

#This tracker is for pcsx-rearmed-libretro issues *only*.

If the issue also occurs in standalone PCSX-ReARMed, the upstream repo is probably a better place for it.

That said, we would suggest that you also try it on other PCSX-based forks such as PCSX Reloaded, PCSX Redux and the upstream version
as well because these share a similar codebase (or derive from it) and are helpful for regression testing, thus making it more 
likely for the bug to be fixed if it is indeed specific to the libretro core only.

## Description

Please describe the issue. If this is a feature request, please add [feature request] to the title of the issue report.

## Steps to reproduce

How can we reproduce this issue? Please include all relevant steps, as well as any settings that you have changed from their default values, particularly including whether you are using the dynarec or interpreter CPU emulation.

## When did the behavior start?

If you can bisect the issue (that is, pinpoint the exact commit that caused the problem), please do. Someone has to do it, and having this already done greatly increases the likelihood that a developer will investigate and/or fix the problem. Barring that, please state the last time it worked properly (if ever).

## Your device/OS/platform/architecture

Such as Android, iOS, Windows 10, Linux, x86_64, a smart toaster, etc.

## Logs (enable file logging and set log levels to DEBUG for core and frontend)

## Screenshots (if needed for visual confirmation)

## Others (save states and/or save files nearest to the affected area, compressed)
