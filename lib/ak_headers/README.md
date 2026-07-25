# WWise Apache-licenced header files

Surprisingly, a lot of the files from the WWise SDK are dual-licenced with a commerical licence
and the Apache-2 licence. It turns out that this is because AudioKinetic wants to make it possible to
write WWise plugins without licence terms getting in the way.

They've got an old copy of the headers on GitHub [1] but they're about 10 years old. Searching GitHub
for AkPlatforms.h finds a few public projects that contain a copy of the SDK. From here you can remove
all the non-Apache-licenced files to get a clean copy we can redistribute.

The verify_licence script reads the headers of all the includes to make sure no proprietary files
slipped through the cracks.

[1] https://github.com/audiokinetic/WwiseIncludes
