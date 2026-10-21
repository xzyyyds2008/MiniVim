#include "Editor.hpp"

#include <algorithm>
#include <cctype>
#include <exception>

//以下提示均可用于Basic部分的实现,部分函数在Advanced部分中需要修改
//在该文件中,有些函数我们完整保留了正确的实现,有些函数去掉了一些,其余的完全需要你自己填写
namespace sjtu {

namespace {
//匿名namespace,提供了只给当前文件使用的辅助函数

std::string Trim(std::string value) {
    while(!value.empty() && std::isspace(value.front())){
        value.erase(value.begin());
    }
    while(!value.empty() &&std::isspace(value.back())){
        value.pop_back();
    }
    return value;
    //去掉字符串两端的空白,保留中间的内容;全部是空白时返回空字符串
    //你可以分别从两端找到第一个非空白字符,注意反向迭代器转回正向迭代器时的边界
}

//判断是否为ASCII可打印字符,Tab由插入模式另外处理
bool IsPrintable(char value) { return value >= 0x20U && value < 0x7FU; }

} // namespace

//用path初始化文件缓冲区,Terminal构造时会准备好终端输入环境
Editor::Editor(const std::filesystem::path& path) : buffer_(path), terminal_() {}

void Editor::Run() {
    //每轮先刷新画面,再读取并处理一个按键;退出循环后清屏
    while (running_) {
        RefreshScreen();
        ProcessKey(terminal_.ReadKey());
    }
    terminal_.ClearScreen();
}

//返回编辑器是否还需要继续运行
bool Editor::IsRunning() const noexcept { return running_; }

void Editor::RefreshScreen() {
    window_.Resize(terminal_.GetScreenSize());
    RenderState state{mode_,command_,message_};
    terminal_.WriteOutput(renderer_.Render(buffer_,window_,state));
    //1. 获取终端大小(GetScreenSize),更新窗口可显示的范围,并让光标落在可见区域内
    //2. 把当前模式、命令和提示打包成RenderState
    //3. 让Renderer生成一帧字符串,再交给Terminal输出
    //这里是你唯一需要调用Terminal中的接口的地方,请调用WriteOutput
}

void Editor::ProcessKey(KeyEvent key) {
    if(key.IsControl('q')){
        running_=false;
        return;
    }
    switch(mode_){
        case Mode::Insert:
            HandleInsert(key);
            break;
        case Mode::CommandLine:
            HandleCommandLine(key);
            break;
        case Mode::Normal:{
            message_.clear();
            Execute(normal_parser_.Feed(key));
            break;
        }
    }
    //1. (可选)先处理所有模式都能使用的Ctrl-Q,用于紧急退出
    //2. 按当前模式分发给命令行或插入模式的处理函数
    //3. Normal模式下清除旧提示,将按键交给parser,再执行生成的Action
}

void Editor::Execute(const EditorAction& action) {
    //根据Action的种类调用对应模块
    switch (action.kind_) {
    case ActionKind::None:
        return;
    case ActionKind::Move:
        if(action.motion_){
            window_.ApplyMotion(buffer_,*action.motion_);
        }
        //交给Window吧
        return;
    case ActionKind::InsertBefore:
        EnterInsert(window_.GetCursor());
        //进入InsertMode,Editor自己就有对应方法
        return;
    case ActionKind::InsertAfter: {
        Position pos=window_.GetCursor();
        std::string line=buffer_.GetLineAt(pos.row_);
        if(!line.empty()){
            pos.column_++;
        }
        EnterInsert(pos);
        //从当前Char之后进入InsertMode
        //特判:如果Buffer表示当前Cursor所在的行是空的怎么办?
        return;
    }
    case ActionKind::EnterCommandLine:
        mode_=Mode::CommandLine;
        command_.clear();
        message_.clear();
        //进入Command Mode
        //记得清空当前的message之类的遗留状态
        return;
    }
}

//Insert模式下Editor对于KeyEvent的处理.
void Editor::HandleInsert(KeyEvent key) {
    switch (key.code_){
        case KeyCode::Escape:
            LeaveInsert();
            return;
        case KeyCode::Enter:{
            Position pos=window_.GetCursor();
            buffer_.SplitLine(pos.row_,pos.column_);
            window_.SetCursor(buffer_,{pos.row_+1,0},true);
            return;
        }
        case KeyCode::Backspace:{
            Position pos=window_.GetCursor();
            if(pos.column_){
                buffer_.EraseCharacter(pos.row_,pos.column_-1);
                window_.SetCursor(buffer_,{pos.row_,pos.column_-1},true);
            }
            else if(pos.row_>0){
                std::size_t prevLen = buffer_.GetLineAt(pos.row_-1).size();
                buffer_.JoinLine(pos.row_-1);
                window_.SetCursor(buffer_,{pos.row_-1,prevLen},true);
            }
            return;
        }
        case KeyCode::Character:{
            if(IsPrintable(key.value_)||key.value_=='\t'){
                Position pos=window_.GetCursor();
                buffer_.InsertCharacter(pos.row_,pos.column_,key.value_);
                window_.SetCursor(buffer_,{pos.row_,pos.column_+1},true);
            }
            return;
        }
        default:
            return;
    }
    //在该函数中你需要同时照顾Buffer和Window的状态
    //1. 若是Escape退出插入模式
    //2. Enter在光标处分行,光标移动到新行开头
    //3. Backspace删除前一个字符;若在行首且不是第一行,则与上一行合并
    //4. 可打印字符和Tab插入当前位置,光标向后移动一列
    //修改内容后记得同步Window中的光标,插入模式允许光标位于line.size()
}
void Editor::EnterInsert(Position position) {
    mode_=Mode::Insert;
    message_.clear();
    window_.SetCursor(buffer_,position,true);
    //切换到Insert模式,设置插入位置并清除旧提示;允许光标停在行尾字符之后
}

void Editor::LeaveInsert() {
    Position pos=window_.GetCursor();
    if(pos.column_>0) pos.column_--;
    mode_=Mode::Normal;
    window_.SetCursor(buffer_,pos,false);
    //从插入位置回到Normal模式的字符位置:不在行首时先左移一列,再限制光标范围
}



void Editor::HandleCommandLine(KeyEvent key) {
    switch(key.code_){
        case KeyCode::Escape:
            LeaveCommandLine();
            return;
        case KeyCode::Enter:
            ExecuteCommandLine();
            return;
        case KeyCode::Delete:
        case KeyCode::Backspace:
            if(!command_.empty()) command_.pop_back();
            return;
        case KeyCode::Character:
            if(IsPrintable(key.value_)) command_.push_back(key.value_);
            return;
        default:
            return;
    }
    //命令内容保存在command_中,不修改Buffer
    //Escape取消命令,Enter执行命令,Backspace/Delete删除末尾字符,可打印字符追加到末尾
    //注意空命令不能再删除字符
}

void Editor::ExecuteCommandLine() {
    std::string cmd=Trim(command_);
    LeaveCommandLine();
    if(cmd.empty()) return;
    if(cmd=="wq" || cmd=="wq!" || cmd=="x"){
        if(SaveBuffer()) running_=false;
    }
    else if(cmd=="q!" || cmd=="quit!"){
        running_=false;
    }
    else if(cmd=="w"){
        SaveBuffer();
    }
    else{
        message_="E: unknown command: "+cmd;
    }
    //1. 保存去掉首尾空白后的命令(用trim),再退出命令行模式,因为退出会清空command_
    //2. 空命令直接返回,否则按第一个空格或Tab拆成命令名和参数
    //3. 在Basic部分中你会发现最后命令就一个命令名,直接根据要求的命令名执行
    //4. 无法识别的命令写入message_,供下一次刷新显示
}

void Editor::LeaveCommandLine() {
    mode_=Mode::Normal;
    command_.clear();
    //恢复Normal模式并清空正在输入的命令
}


bool Editor::SaveBuffer(const std::filesystem::path& path) {
    if(path.empty()) buffer_.Save();
    else buffer_.SaveAs(path);
    message_="\""+buffer_.GetDisplayName()+"\" "+std::to_string(buffer_.GetLineCount())+"L written";
    return true;
    //1. path为空时调用Save,否则调用SaveAs
    //2. 捕获保存时的异常,把错误写入message_并返回false
    //3. 成功后生成包含文件名和行数的提示,返回true,供wq判断是否可以退出
}

} // namespace sjtu
