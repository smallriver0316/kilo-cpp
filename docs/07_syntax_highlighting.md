---
layout: page
---

# Chapter 7: Syntax Highlighting

In this chapter, we'll add syntax highlighting to our text editor. Syntax highlighting makes code more readable by colorizing different parts of the code according to their function or meaning. We'll implement a flexible system that can be extended to support various programming languages, starting with C/C++.

## Colorful Digits

Let's start with something simple: highlighting numbers. We'll create an enumeration to represent different syntax elements that can be highlighted.

First, we need to define the types of syntax elements we want to highlight:

```cpp
enum class EditorHighlight : unsigned char
{
    NORMAL = 0,
    NUMBER,
    MATCH
};
```

We've defined three types: `NORMAL` for regular text, `NUMBER` for numeric literals, and `MATCH` for search matches (which we implemented in the previous chapter).

Now let's add an array to each `EditorRow` to store the highlighting information for each character:

```cpp
struct EditorRow
{
    int idx;
    std::string row;
    std::string rendered;
    std::vector<EditorHighlight> hl;
    bool hl_open_comment;
};
```

We added a vector `hl` that will store an `EditorHighlight` value for each character in the `rendered` string, indicating how that character should be highlighted. We also added a `hl_open_comment` boolean that we'll use later for multi-line comments.

## Updating Syntax Highlighting

Next, we need a function to update the syntax highlighting for a row. We'll start by highlighting numbers:

```cpp
void Editor::updateSyntax(EditorRow &erow)
{
    erow.hl.resize(erow.rendered.size(), EditorHighlight::NORMAL);

    if (!m_syntax)
        return;

    int prev_sep = 1;  // Consider start of line as a separator

    int i = 0;
    while (i < static_cast<int>(erow.rendered.size())) {
        char c = erow.rendered[i];
        unsigned char prev_hl = (i > 0) ? static_cast<unsigned char>(erow.hl[i - 1])
                                         : static_cast<unsigned char>(EditorHighlight::NORMAL);

        // Highlight numbers
        if ((isdigit(c) && (prev_sep || prev_hl == static_cast<unsigned char>(EditorHighlight::NUMBER))) ||
            (c == '.' && prev_hl == static_cast<unsigned char>(EditorHighlight::NUMBER))) {
            erow.hl[i] = EditorHighlight::NUMBER;
            i++;
            prev_sep = 0;
            continue;
        }

        prev_sep = isSeparator(c);
        i++;
    }
}
```

This function iterates through each character in the row and determines if it should be highlighted as a number. We consider a character to be part of a number if it's a digit that follows a separator or another number, or if it's a decimal point that follows a number.

We need to define the `isSeparator` function to determine if a character is a separator:

```cpp
bool isSeparator(int c)
{
    return std::isspace(c) || c == '\0' || std::strchr(",.()+-/*=~%<>[];", c) != NULL;
}
```

A separator is any whitespace, null character, or one of the specified punctuation characters.

## Converting Highlight Values to Colors

Now we need a function to convert our `EditorHighlight` values to ANSI color codes:

```cpp
int Editor::convertSyntaxToColor(EditorHighlight hl)
{
    switch (hl)
    {
    case EditorHighlight::NUMBER:
        return 31;  // Red
    case EditorHighlight::MATCH:
        return 34;  // Blue
    default:
        return 37;  // White (normal)
    }
}
```

## Displaying Highlighted Text

Now we need to modify our `drawRows` function to display the highlighted text:

```cpp
void Editor::drawRows(std::string &s)
{
    for (int y = 0; y < m_screenrows; ++y) {
        auto filerow = y + m_rowoff;
        if (filerow >= static_cast<int>(m_rows.size())) {
            if (m_rows.empty() && y == m_screenrows / 3) {
                std::string welcome = "Kilo++ editor -- version " + std::string(KILO_VERSION);
                if (welcome.size() > static_cast<std::size_t>(m_screencols))
                    welcome.resize(m_screencols);

                auto padding = (static_cast<std::size_t>(m_screencols) - welcome.size()) / 2;
                if (padding) {
                    s += "~";
                    padding--;
                }

                while (padding--)
                    s += " ";

                s += welcome;
            }
            else {
                s += "~";
            }
        }
        else {
            int len = static_cast<int>(m_rows[filerow].rendered.size()) - m_coloff;
            if (len < 0)
                len = 0;

            int current_color = -1;
            for (int i = 0; i < len; ++i) {
                const auto &c = m_rows[filerow].rendered[m_coloff + i];
                const auto &hl = m_rows[filerow].hl[m_coloff + i];
                if (std::iscntrl(c)) {
                    char sym = c <= 26 ? '@' + c : '?';
                    s += "\x1b[7m";
                    s += std::string(1, sym);
                    s += "\x1b[m";
                    if (current_color != -1)
                        s += "\x1b[" + std::to_string(current_color) + "m";
                }
                else if (hl == EditorHighlight::NORMAL) {
                    if (current_color != -1) {
                        s += "\x1b[39m";
                        current_color = -1;
                    }
                    s += std::string(1, c);
                }
                else {
                    const auto color = convertSyntaxToColor(hl);
                    if (current_color != color) {
                        current_color = color;
                        std::vector<char> buf(16);
                        snprintf(buf.data(), buf.size(), "\x1b[%dm", color);
                        s += buf.data();
                    }
                    s += std::string(1, c);
                }
            }
            s += "\x1b[39m";
        }

        s += "\x1b[K\r\n";
    }
}
```

This updated function keeps track of the current color as it draws each character, and only outputs a color escape sequence when the color changes.

## Integrating Syntax Highlighting

Let's integrate our syntax highlighting system by calling `updateSyntax` when a row is updated:

```cpp
void Editor::updateRow(EditorRow &erow)
{
    std::string render = "";
    for (const auto &c : erow.row) {
        if (c == '\t') {
            render += " ";
            while (render.size() % KILO_TAB_STOP != 0)
                render += " ";
        }
        else {
            render += c;
        }
    }

    render += "\0";
    erow.rendered = render;
    updateSyntax(erow);
}
```

## Language Detection

Now let's create a system to detect the file type based on its extension, and apply different syntax highlighting rules accordingly.

First, let's define a structure to hold syntax highlighting rules:

```cpp
struct EditorSyntax
{
    std::string filetype;
    std::vector<std::string_view> filematch;
    std::vector<std::string_view> keywords;
    std::string singleline_comment_start;
    std::string multiline_comment_start;
    std::string multiline_comment_end;
    int32_t flags;
};
```

And define some flags for additional highlighting options:

```cpp
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
```

Now let's define syntax highlighting rules for C/C++:

```cpp
const std::vector<std::string_view> C_HL_EXTENSIONS = {".c", ".h", ".cpp", ".hpp"};
const std::vector<std::string_view> C_HL_KEYWORDS = {
    "switch", "if", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "const",
    "enum", "class", "case", "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "auto|", "void|"};

const EditorSyntax HLDB[] = {
    {"c",
     C_HL_EXTENSIONS,
     C_HL_KEYWORDS,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
};
```

Note that we mark certain keywords with a pipe character (`|`) at the end. These are type names that we'll highlight differently than other keywords.

Let's add more highlight types:

```cpp
enum class EditorHighlight : unsigned char
{
    NORMAL = 0,
    COMMENT,
    ML_COMMENT,
    KEYWORD1,
    KEYWORD2,
    STRING,
    NUMBER,
    MATCH
};
```

And update our `convertSyntaxToColor` function:

```cpp
int Editor::convertSyntaxToColor(EditorHighlight hl)
{
    switch (hl)
    {
    case EditorHighlight::COMMENT:
    case EditorHighlight::ML_COMMENT:
        return 36;  // Cyan
    case EditorHighlight::KEYWORD1:
        return 33;  // Yellow
    case EditorHighlight::KEYWORD2:
        return 32;  // Green
    case EditorHighlight::STRING:
        return 35;  // Magenta
    case EditorHighlight::NUMBER:
        return 31;  // Red
    case EditorHighlight::MATCH:
        return 34;  // Blue
    default:
        return 37;  // White (normal)
    }
}
```

## File Type Detection

Now let's implement a function to detect the file type based on its extension:

```cpp
void Editor::selectSyntaxHighlight()
{
    m_syntax = nullptr;
    if (m_filename.empty())
        return;

    const auto ext = m_filename.rfind('.');

    for (const auto &syntax : HLDB) {
        for (const auto &fm : syntax.filematch) {
            int is_ext = (fm[0] == '.');
            if ((is_ext && ext != std::string::npos && !m_filename.compare(ext, fm.size(), fm)) ||
                (!is_ext && m_filename.find(fm) != std::string::npos)) {
                m_syntax = std::make_shared<EditorSyntax>(syntax);

                for (auto &row : m_rows)
                    updateSyntax(row);

                return;
            }
        }
    }
}
```

This function checks if the filename ends with one of the extensions in our `filematch` array, or if it contains one of the patterns when the pattern doesn't start with a period.

## Adding String Highlighting

Let's update our `updateSyntax` function to handle string literals:

```cpp
void Editor::updateSyntax(EditorRow &erow)
{
    erow.hl.resize(erow.rendered.size(), EditorHighlight::NORMAL);

    if (!m_syntax)
        return;

    const auto &keywords = m_syntax->keywords;

    bool prev_sep = true;
    int in_string = 0;

    std::size_t i = 0;
    while (i < erow.rendered.size()) {
        const auto c = erow.rendered[i];
        EditorHighlight prev_hl = i > 0 ? erow.hl[i - 1] : EditorHighlight::NORMAL;

        // String highlighting
        if (m_syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                erow.hl[i] = EditorHighlight::STRING;

                if (c == '\\' && i + 1 < erow.rendered.size()) {
                    erow.hl[i + 1] = EditorHighlight::STRING;
                    i += 2;
                    continue;
                }

                if (c == in_string)
                    in_string = 0;

                prev_sep = true;
                i++;
                continue;
            }
            else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    erow.hl[i] = EditorHighlight::STRING;
                    i++;
                    continue;
                }
            }
        }

        // Number highlighting
        if (m_syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || static_cast<unsigned char>(prev_hl) == static_cast<unsigned char>(EditorHighlight::NUMBER))) ||
                (c == '.' && static_cast<unsigned char>(prev_hl) == static_cast<unsigned char>(EditorHighlight::NUMBER))) {
                erow.hl[i] = EditorHighlight::NUMBER;
                i++;
                prev_sep = false;
                continue;
            }
        }

        prev_sep = isSeparator(c);
        i++;
    }
}
```

## Adding Keyword Highlighting

Now let's add keyword highlighting:

```cpp
void Editor::updateSyntax(EditorRow &erow)
{
    erow.hl.resize(erow.rendered.size(), EditorHighlight::NORMAL);

    if (!m_syntax)
        return;

    const auto &keywords = m_syntax->keywords;

    bool prev_sep = true;
    int in_string = 0;

    std::size_t i = 0;
    while (i < erow.rendered.size()) {
        const auto c = erow.rendered[i];
        EditorHighlight prev_hl = i > 0 ? erow.hl[i - 1] : EditorHighlight::NORMAL;

        // String highlighting
        // ...

        // Number highlighting
        // ...

        // Keyword highlighting
        if (prev_sep) {
            auto kwitr = keywords.begin();
            for (; kwitr != keywords.end(); ++kwitr) {
                auto kw = *kwitr;
                int klen = kw.size();
                bool is_kw2 = kw[klen - 1] == '|';
                if (is_kw2)
                    kw = kw.substr(0, --klen);

                if (!erow.rendered.compare(i, klen, kw) &&
                    isSeparator(erow.rendered[i + klen])) {
                    std::fill(
                        erow.hl.begin() + i,
                        erow.hl.begin() + i + klen,
                        is_kw2 ? EditorHighlight::KEYWORD2 : EditorHighlight::KEYWORD1);
                    i += klen;
                    break;
                }
            }
            if (kwitr != keywords.end()) {
                prev_sep = false;
                continue;
            }
        }

        prev_sep = isSeparator(c);
        i++;
    }
}
```

## Adding Comment Highlighting

Finally, let's add support for highlighting comments:

```cpp
bool isSLCommentStarted(EditorRow &erow, const std::string &comment_start_kw, std::size_t pos)
{
    if (!erow.rendered.compare(pos, comment_start_kw.size(), comment_start_kw)) {
        std::fill(
            erow.hl.begin() + pos,
            erow.hl.end(),
            EditorHighlight::COMMENT);
        return true;
    }
    return false;
}

bool isMLCommentStarted(EditorRow &erow, const std::string &comment_start_kw, std::size_t pos)
{
    if (!erow.rendered.compare(pos, comment_start_kw.size(), comment_start_kw)) {
        std::fill(
            erow.hl.begin() + pos,
            erow.hl.begin() + pos + comment_start_kw.size(),
            EditorHighlight::ML_COMMENT);
        return true;
    }
    return false;
}

bool isMLCommentEnded(EditorRow &erow, const std::string &comment_end_kw, std::size_t pos)
{
    if (!erow.rendered.compare(pos, comment_end_kw.size(), comment_end_kw)) {
        std::fill(
            erow.hl.begin() + pos,
            erow.hl.begin() + pos + comment_end_kw.size(),
            EditorHighlight::ML_COMMENT);
        return true;
    }
    return false;
}

void Editor::updateSyntax(EditorRow &erow)
{
    erow.hl.resize(erow.rendered.size(), EditorHighlight::NORMAL);

    if (!m_syntax)
        return;

    const auto &keywords = m_syntax->keywords;
    const auto &scs = m_syntax->singleline_comment_start;
    const auto &mcs = m_syntax->multiline_comment_start;
    const auto &mce = m_syntax->multiline_comment_end;

    bool prev_sep = true;
    int in_string = 0;
    bool in_comment = (erow.idx > 0 && m_rows[erow.idx - 1].hl_open_comment);

    std::size_t i = 0;
    while (i < erow.rendered.size()) {
        const auto c = erow.rendered[i];
        EditorHighlight prev_hl = i > 0 ? erow.hl[i - 1] : EditorHighlight::NORMAL;

        // Single-line comment
        if (!scs.empty() && !in_string && !in_comment) {
            if (isSLCommentStarted(erow, scs, i))
                break;
        }

        // Multi-line comment
        if (!mcs.empty() && !mce.empty() && !in_string) {
            if (in_comment) {
                erow.hl[i] = EditorHighlight::ML_COMMENT;
                if (isMLCommentEnded(erow, mce, i)) {
                    i += mce.size();
                    in_comment = false;
                    prev_sep = true;
                }
                else {
                    i++;
                }
                continue;
            }
            else if (isMLCommentStarted(erow, mcs, i)) {
                i += mcs.size();
                in_comment = true;
                continue;
            }
        }

        // String highlighting
        // ...

        // Number highlighting
        // ...

        // Keyword highlighting
        // ...

        prev_sep = isSeparator(c);
        i++;
    }

    bool is_changed = erow.hl_open_comment != in_comment;
    erow.hl_open_comment = in_comment;
    if (is_changed && erow.idx + 1 < static_cast<int>(m_rows.size()))
        updateSyntax(m_rows[erow.idx + 1]);
}
```

We've added logic to handle single-line and multi-line comments. We also keep track of whether a multi-line comment is open at the end of the row, and if that state changes, we recursively update the syntax of the next row.

## Putting It All Together

Let's call our `selectSyntaxHighlight` function at the appropriate times:

```cpp
void Editor::open(const char *filename)
{
    if (!std::filesystem::exists(filename)) {
        std::ofstream file(filename);
        if (!file)
            terminal_manager::die("create a new file");
    }

    m_filename = std::string(filename);

    selectSyntaxHighlight();

    // ...
}

void Editor::save()
{
    if (m_filename.empty()) {
        m_filename = fromPrompt("Save as: %s (ESC to cancel)");
        if (m_filename.empty()) {
            setStatusMessage("Save aborted");
            return;
        }
        selectSyntaxHighlight();
    }

    // ...
}
```

## Displaying the File Type in the Status Bar

Finally, let's update our status bar to display the file type:

```cpp
void Editor::drawStatusBar(std::string &s)
{
    s += "\x1b[7m";

    std::stringstream ss, rss;
    ss << (m_filename.empty()
             ? "[No Name]"
             : m_filename.substr(0, std::min(static_cast<int>(m_filename.size()), FILENAME_DISPLAY_LEN)))
     << " - "
     << m_rows.size()
     << " lines"
     << (m_dirty ? "(modified)" : "");
    int len = std::min(static_cast<int>(ss.str().size()), m_screencols);

    rss << (m_syntax ? m_syntax->filetype : "no ft")
        << " | "
        << m_cy + 1
        << "/"
        << m_rows.size();
    int rlen = rss.str().size();

    s += ss.str().substr(0, len);
    while (len < m_screencols) {
        if (m_screencols - len == rlen) {
            s += rss.str();
            break;
        }
        s += " ";
        len++;
    }

    s += "\x1b[m\r\n";
}
```

## Conclusion

We've now implemented a powerful syntax highlighting system for our text editor. It can highlight numbers, strings, keywords, and comments, and it's flexible enough to support different programming languages.

Let's wrap up our kilo++ text editor!

```cpp
int main(int argc, char **argv)
{
    Editor editor;
    editor.run(argc, argv);
    return 0;
}
```

Congratulations! You've built a text editor with syntax highlighting from scratch using modern C++. This is just the beginning - there are many more features you could add, such as more language support, auto-indentation, code folding, and more.

Happy coding!

[<- previous chapter](06_search)
