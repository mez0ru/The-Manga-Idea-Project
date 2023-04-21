# Manga Idea
An attempt at rewriting [Komga](https://github.com/gotson/komga/) in C++. Komga is extremely versatile Comic server.\
However, since it uses JVM runtime, it requires huge amount of memory.\
From my experience, it's really fast, I would say beating Komga's performance is a challenge, even\
with a systems langauge like C++. However, I could achieve almost double the performance after trying several approaches.\
If you are a systems administrator, I would strongly advise you to stick with Komga, since it has a lot of features, and battle tested.\
The intended audience is people who want to run a local server on their own PC, such as myself.

# Goals
1. Acheive good performance like Komga.
2. Good reading experience on the frontend.
3. Stability, and rare crashes.
4. Less than 10mb of memory usage.


# Status
The project is in active development, still incomplete.
Around 30% is done? But it's not even usable.

# Dependencies
use VCPKG to download these dependencies:
1. RESTinio
2. LibArchive
3. Boost JSON
4. json_dto
5. libjxl (to decode image properties of JPEG XL)
6. sqlite_modern_cpp
7. Magick++ (Download the binery from magick website and make sure to include development libraries during installation).
8. fmt
9. jwt-cpp\
\
That's a long list, planning on shortening it later, development speed is the most important thing for now 🤩

# Stack
1. React + MUI
2. C++ RESTinio
