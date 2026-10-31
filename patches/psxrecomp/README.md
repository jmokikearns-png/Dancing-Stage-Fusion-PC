# PSXRecomp changes

The game core is built with [PSXRecomp](https://github.com/mstan/psxrecomp)
(PolyForm Noncommercial 1.0.0). These patches add what this project needs on
top of it: running as a library inside StepMania (embedded mode), handing the
picture to the host, the PlayStation's exact video timing, render passes for
SMOOTH ARROWS, and keeping the core's files in the host's data folder.

Base: upstream commit `2a4824fcb897cb635438958f0c8cffb5a27df656`.

```
git clone https://github.com/mstan/psxrecomp
cd psxrecomp
git checkout 2a4824fcb897cb635438958f0c8cffb5a27df656
git am --keep-cr /path/to/patches/psxrecomp/*.patch
```

`--keep-cr` matters: some of the files use Windows line endings.
