#include <fstream>

#include "Buffer.hpp"

namespace sjtu {

Buffer::Buffer(const std::filesystem::path& path){
    path_=path;
    if(path.empty()){
        lines_.push_back("");
        return;
    }
    std::ifstream in(path);
    std::string line;
    while(std::getline(in,line)){
        lines_.push_back(line);
    }
    if(lines_.empty()){
        lines_.push_back("");
    }
    modified_=false;
    //从path指向的文件构造Buffer,你需要打开文件并且把文件内容填充进Buffer,并正确初始化一些状态.
    //注意path可能为空的边界情况
}

Buffer::Buffer(std::vector<std::string> lines, std::filesystem::path path) {
    for(int i=0;i<lines.size();i++){
        lines_.push_back(lines[i]);
    }
    path_=path;
    if(lines_.empty()){
        lines_.push_back("");
    }
    modified_=false;
}

std::size_t Buffer::GetLineCount() const {
    //返回文件行数
    return lines_.size();
}

const std::string& Buffer::GetLineAt(std::size_t row) const {
    return lines_.at(row);
}


std::string Buffer::GetDisplayName() const {
    if(path_.empty()) return "[No Name]";
    return path_.filename().string();
}

bool Buffer::IsModified() const {
    return modified_;
}

void Buffer::InsertCharacter(std::size_t row, std::size_t column, char value) {
    if(row>=lines_.size()) return;
    std::string& line=lines_[row];
    if(column>=line.size()) column=line.size();
    line.insert(line.begin()+static_cast<std::ptrdiff_t>(column),value);
    modified_=true;
}

void Buffer::EraseCharacter(std::size_t row, std::size_t column) {
    if(row>=lines_.size()) return;
    std::string& line=lines_[row];
    if(column>=line.size()) return;
    line.erase(line.begin()+static_cast<std::ptrdiff_t>(column));
   //在第row行第col列删除一个value
    modified_=true;
}

void Buffer::SplitLine(std::size_t row, std::size_t column) {
    if(row>=lines_.size()) return;
    std::string& line=lines_[row];
    if(column>line.size()) column=line.size();
    std::string left=line.substr(0,column);
    std::string right=line.substr(column);
    lines_[row]=left;
    lines_.insert(lines_.begin()+static_cast<std::ptrdiff_t>(row+1),right);
    //在第row行第col列分割,即在此处敲了回车键
    modified_=true;
}

void Buffer::JoinLine(std::size_t row) {
    if(row+1>=lines_.size()) return;
    lines_[row]+=lines_[row+1];
    lines_.erase(lines_.begin()+static_cast<std::ptrdiff_t>(row+1));
   //把第row + 1行合并进第row行
    modified_=true;
}

void Buffer::Save() {
    if(path_.empty()) return;
    WriteTo(path_);
   //把文件内容保存, 直接调用WriteTo方法
   modified_=false;
}

void Buffer::SaveAs(const std::filesystem::path& path) {
    WriteTo(path);
    path_=path;
  modified_=false;
}


void Buffer::WriteTo(const std::filesystem::path& path) const {
    std::ofstream out(path);
    if(!out) return;
    for(std::size_t i=0;i<lines_.size();i++){
        out<<lines_[i];
        if(i+1<lines_.size())
            out<<'\n';
    }
   //实际将缓冲区中的内容写入path指向的文件中
}

} // namespace sjtu
