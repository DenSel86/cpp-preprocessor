#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool OpenFile(ifstream& ifs, const path& path_filename, const path& current_path, const vector<path>& include_directories) {
    path path_full_filename;
    if (!current_path.empty()) {
        path_full_filename = current_path / path_filename;
        ifs.open(path_full_filename.string());
        if (ifs.is_open()) {
            return true;
        }
    }
    for (const path& include_directory : include_directories) {
        path_full_filename = include_directory / path_filename;
        ifs.open(path_full_filename.string());
        if (ifs.is_open()) {
            return true;
        }
    }
    return false;
}

bool PreprocessRecursion(ifstream& ifs, ofstream& ofs, const path& path_parent_file, const vector<path>& include_directories) {
    static regex reg_include_quotes(R"raw(\s*#\s*include\s*"([^"]*)"\s*)raw");
    static regex reg_include_brackets(R"raw(\s*#\s*include\s*<([^>]*)>\s*)raw");

    const path parent_path = path_parent_file.parent_path();

    string str;
    int number_line = 0;
    while (getline(ifs, str)) {
        ++number_line;
        ifstream ifs_next;
        path current_dir;
        smatch results_of_match;
        if (regex_match(str, results_of_match, reg_include_quotes)) {
            // указываем текущую директорию для поиска файла
            current_dir = parent_path;
        } else if(regex_match(str, results_of_match, reg_include_brackets)) {
            // текущая директория для поиска файла не присваивается, игнорируется
        } else {
            ofs << str << endl;
        }
        if (!results_of_match.empty()) {
            path path_next_file = string(results_of_match[1]);
            if (OpenFile(ifs_next, path_next_file, current_dir, include_directories)) {
                 if(!PreprocessRecursion(ifs_next, ofs, current_dir / path_next_file, include_directories)) {
                    return false;
                }
            } else {
                cout << "unknown include file " << path_next_file.string() 
                     << " at file " << path_parent_file.string() 
                     << " at line "s << number_line << endl;
                return false;
            }
        }
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream ifs(in_file.string());
    if (!ifs.is_open()) {
        return false;
    }

    ofstream ofs(out_file.string());
    if (!ifs.is_open()) {
        return false;
    }

    return PreprocessRecursion(ifs, ofs, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}