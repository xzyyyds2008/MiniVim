#include "Window.hpp"
#include "TextLayout.hpp"

#include <algorithm>
#include <cctype>

namespace sjtu {


void Window::Resize(ScreenSize terminal_size) {
    viewport_.rows_=terminal_size.rows_>1?terminal_size.rows_-1:1;
    viewport_.columns_=terminal_size.columns_>0?terminal_size.columns_:1;
    viewport_.top_ = 0;
    viewport_.left_ = 0;
    //底部留一行给命令或提示,其余作为正文区域;正文行数和列数都至少取1
    //修改视口即可
}

void Window::ApplyMotion(const Buffer& buffer, Motion motion) {
    if(motion==Motion::Left) MoveLeft(buffer, 1);
    else if(motion==Motion::Right) MoveRight(buffer, 1);
    else if (motion==Motion::Up) MoveUp(buffer, 1);
    else if (motion==Motion::Down) MoveDown(buffer, 1);
    EnsureCursorVisible(buffer);
    //1. 根据方向调用对应的移动函数,Basic中每次移动一步,在Advanced中你可以改变count/添加别的case
    //2. 将行列限制在Normal模式的合法范围内(Buffer应始终至少有一行)
    //3. 调整视口,让移动后的光标可见(EnsureCursorVisible)
}

void Window::EnsureCursorVisible(const Buffer& buffer) {
    if(cursor_.row_<viewport_.top_) viewport_.top_=cursor_.row_;
    else if(cursor_.row_>=viewport_.top_+viewport_.rows_) viewport_.top_=cursor_.row_-viewport_.rows_+1;
    const auto& line=buffer.GetLineAt(cursor_.row_);
    size_t col=BufferColumnToRenderColumn(line,cursor_.column_);
    if(col<viewport_.left_) viewport_.left_=col;
    else if(col>=viewport_.left_+viewport_.columns_) viewport_.left_=col-viewport_.columns_+1;
    //1. 光标高于或低于可见区域时,调整top_,使光标刚好进入区域
    //2. 把光标的字符下标换算成显示列,再用相同思路调整left_
}

const Position& Window::GetCursor() const {
    //返回当前光标位置的只读引用
    return cursor_;
}

const Viewport& Window::GetViewport() const {
    //返回当前可见区域的只读引用,供Renderer绘制
   return viewport_;
}


void Window::SetCursor(const Buffer& buffer, Position position, bool allow_line_end) {
    if(position.row_>=buffer.GetLineCount()){
        position.row_=buffer.GetLineCount()-1;
    }
    std::string line=buffer.GetLineAt(position.row_);
    size_t maxcol=allow_line_end?line.size():(line.empty()?0:line.size()-1);
    if(position.column_>maxcol) position.column_=maxcol;
    cursor_=position;
    desired_column_=BufferColumnToRenderColumn(line,cursor_.column_);
    EnsureCursorVisible(buffer);
    //1. 先限制行号,再根据该行长度和allow_line_end限制列号
    //2. 用新位置更新上下移动时的目标显示列
    //3. 调整视口,保证光标可见
}


void Window::MoveLeft(const Buffer& buffer, std::size_t count) {
    cursor_.column_=cursor_.column_>=count?cursor_.column_-count:0;
    desired_column_=BufferColumnToRenderColumn(buffer.GetLineAt(cursor_.row_),cursor_.column_);
    //向左移动count个字符,最多到行首,并更新目标显示列
}

void Window::MoveRight(const Buffer& buffer, std::size_t count) {
    std::string line=buffer.GetLineAt(cursor_.row_);
    size_t maxcol=line.empty()?0:line.size()-1;
    cursor_.column_=std::min(cursor_.column_+count,maxcol);
    desired_column_=BufferColumnToRenderColumn(line,cursor_.column_);
    //向右移动count个字符,最多到最后一个字符,并更新目标显示列
}


void Window::MoveUp(const Buffer& buffer, std::size_t count) {
    cursor_.row_=cursor_.row_>=count?cursor_.row_-count:0;
    std::string line=buffer.GetLineAt(cursor_.row_);
    cursor_.column_=RenderColumnToBufferColumn(line,desired_column_);
    if(line.empty()){
        cursor_.column_=0;
    } 
    else if (cursor_.column_>=line.size()){
        cursor_.column_=line.size()-1;
    }
    //先算目标行,最多到第一行,再将期望的显示列换算成目标行的字符下标
    //经过短行时不要更新desired_screen_column_,这样继续移动到长行时能回到原来的列
}

void Window::MoveDown(const Buffer& buffer, std::size_t count) {
    size_t maxrow=buffer.GetLineCount()-1;
    cursor_.row_=std::min(cursor_.row_+count,maxrow);
    std::string line=buffer.GetLineAt(cursor_.row_);
    cursor_.column_=RenderColumnToBufferColumn(line,desired_column_);
    if(line.empty()){
        cursor_.column_=0;
    } 
    else if (cursor_.column_>=line.size()){
        cursor_.column_=line.size()-1;
    }
    //先算目标行,最多到最后一行,再根据desired_screen_column_寻找目标字符
    //与向上移动一样,保留期望显示列
}

} // namespace sjtu
