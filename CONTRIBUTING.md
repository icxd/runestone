# Contributing to Runestone

Thanks for your interest in contributing to Runestone — a modern compiler backend written in pure C. All contributions are welcome, whether it's bug reports, code, documentation, or suggestions.

---

## 📂 How to Contribute

### 🐞 Report Bugs

If you find a bug:
- Open a [GitHub issue](https://github.com/yourusername/runestone/issues)
- Include a clear description of the problem
- Add steps to reproduce it

### ✨ Suggest Features

- Open an issue tagged as `enhancement`
- Describe the motivation behind the feature
- Mention any prior art or inspiration if relevant

### 🔧 Code Contributions

Before submitting code:
- Fork the repo and make your changes on a **new branch**
- Keep commits small and focused
- Use clear commit messages

Once done:
1. Run tests (if available)
2. Make sure the code compiles using `make`
3. Submit a **pull request** with a summary of changes

---

## 🧑‍💻 Code Style

- Stick to C99 or newer (no C++ or GCC extensions)
- Use 2 spaces for indentation, no tabs
- Keep functions short and focused
- Comment complex logic clearly

Naming:
- `snake_case` for functions and variables
- `ALL_CAPS` for macros and constants
- Prefix all functions with `rs_` (e.g. `rs_build_instr`)

---

## 📚 Documentation

- All exported functions must have a Doxygen-style comment.
- Update any relevant examples or internal docs when changing APIs.

---

## 🛠 Build

To compile the project:

```sh
make clean && make
```

---

🧾 License

By contributing, you agree that your code will be released under the same GNU GPLv3 license as the project.

---

Thanks again for helping make Runestone better!

