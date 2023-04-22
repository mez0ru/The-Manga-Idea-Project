# Manga Idea
A manga server, just like [Komga](https://github.com/gotson/komga/) made in C++. Komga is extremely versatile Comic server.\
It has a wide range of features, aside from being memory hungry, it's extremely good at what it does.\
This is just a pet project that I want to do to replace it, since I use it daily to read manga.\
There are missing features from Komga which are important for me, which made me motivated to work on this project.\
However, keep in mind that this is totally different project from Komga, I don't plan on making everything work the same as Komga.\
But I strive to nonetheless. 

# Goals
1. Achieve good performance like Komga.
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
This project uses C++17.\
That's a long list, planning on shortening it later, development speed is the most important thing for now 🤩

# Stack
1. React + MUI
2. RESTinio Backend
