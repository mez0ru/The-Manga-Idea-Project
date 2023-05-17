# Manga Idea
A manga server I'm working on just for fun, just like [Komga](https://github.com/gotson/komga/) made in C++.\
It's made with the goal to be as feature rich as Komga, while being extremely efficient in memory/cpu usage.\
I plan on adding features that Komga simply won't support such as simple folders instead of pdf/archives.\
Future plans could include making an extension for tachiyomi to be used with the server, however it's currently low priority.\
I made it with the intent to learn C++, so expect low quality code. Pull requests and enhancements are welcome!

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
9. jwt-cpp
10. openssl\
\
This project uses C++17.\
That's a long list, planning on shortening it later, development speed is the most important thing for now 🤩

# Stack
1. React + MUI
2. RESTinio Backend
