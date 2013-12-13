////////////////////////////////////////////////////////////////////////////////
//                      Basic Controls
////////////////////////////////////////////////////////////////////////////////

// Define some shorthand names for various function-pointer types. 
// If you do not understand this weird C syntax, that is totally okay.
typedef void (*functionptr)();                    // pointer type for:  void foo()
typedef void (*functionptr_int)(int);             // pointer type for:  void foo(int index)
typedef void (*functionptr_bool)(bool);           // pointer type for:  void foo(bool state)
typedef void (*functionptr_float)(float);         // pointer type for:  void foo(float value)
typedef void (*functionptr_string)(const char*);  // pointer type for:  void foo(const char* text)
typedef void (*functionptr_intint)(int,int);      // pointer type for:  void foo(int a, int b)
typedef void (*functionptr_voidptr)(void*);       // pointer type for:  void foo(void* arg)
//
// For the curious, here's a ridiculously brief description of function pointers.
// In C/C++, functions such as
//    void foo1(int x) { cout <<  x; }
//    void foo2(int x) { cout << -x; }
// can be called by typing foo1(5) later on. Something else you can do with a 
// function name is to say &foo1. In English, &foo1 means "don't call foo1, just 
// give me the address of foo1's code." This makes sense because, just like
// variables, each chunk of code takes up memory and therefore has a location.
// For example, given foo1 and foo2 above we can later write
//    functionptr_int f = &foo1;  // f points to code of foo1
//    f(5);                       // calls foo1(5)
//    f = &foo2;                  // f points to code of foo2
//    f(5);                       // calls foo2(5)
// Above, f is a variable and takes 4 bytes just like any other pointer would.
// The type of f is "pointer to any function that takes one int argument."
//


// CreateTextLabel:
//    Creates a text label that sits "on top" any drawing that's going on in the 
//    main window. Returns a unique integer identifying that label (the label's 
//    "handle"). Useful for putting beside other controls such as TextBox.
//
// Example:
//    void main() {
//        SetWindowVisible(true);
//        int label = CreateTextLabel("Muaha you can't draw on top of me!");
//        while (!WasWindowClosed()) { }   // Wait for user to close main window.
//        DeleteControl(label);
//    }
//
int CreateTextLabel(const char* text);
void SetTextLabelString(int handle, const char* text);

// CreateButton:
//    Creates a Button 'control' inside the window and returns
//    a unique integer identifying that button (the button's "handle"). 
//    When clicked, the button calls whatever function you passed as onClick. 
//
// Example:
//    void tickle() { 
//        cout << "That tickles!" << endl;
//    } 
//
//    void main() {
//        SetWindowVisible(true);
//        int button = CreateButton("I'm a button", tickle);
//        while (!WasWindowClosed()) { }
//        DeleteControl(button);
//    }
//
int CreateButton(const char* text, functionptr onClick);

// CreateCheckBox:
//    Creates a checkbox control with text beside it. 
//    Each time the user checks/unchecks, the checkbox 
//    calls whatever function you passed as onStateChanged.
//
// Example:
//    void moodSwing(bool isHappy) { 
//        if (isHappy) cout << "Yes, I am." << endl;
//        else         cout << "No, I am not." << endl;
//    } 
//
//    void main() {
//        SetWindowVisible(true); 
//        int checkBox = CreateCheckBox("Are you happy?", true, &moodSwing);
//        while (!WasWindowClosed()) { }
//        DeleteControl(checkBox);
//    }
//
int CreateCheckBox(const char* text, bool checked, functionptr_bool onStateChanged);
bool GetCheckBoxState(int handle);
void SetCheckBoxState(int handle, bool checked);

// CreateTextBox:
//    Creates a textbox control. Each time the user edits the text,
//    the textbox calls whatever function you passed as onStringChanged.
//
// Example:
//    void movieChanged(const char* movieString) { 
//        cout << "Your favourite movie is " << movieString << endl;
//    } 
//
//    void main() {
//        SetWindowVisible(true); 
//        int movieBox = CreateTextBox("Being John Malkovich", &movieChanged);
//        while (!WasWindowClosed()) { }
//        DeleteControl(movieBox);
//    }
//
int CreateTextBox(const char* text, functionptr_string onStringChanged);
const char* GetTextBoxString(int handle);
void SetTextBoxString(int handle, const char* text);

// CreateDropList:
//    Creates a drop-list control. Each item in the list displays
//    a text string. Each time the user selects a different item from the
//    list, the list calls whatever function you passed as 'onSelectionChanged'.
//
// Example:
//    const char* candidates[] = { "Obama", "McCain", "Nader", "Barr" };
//
//    void vote(int index) { 
//        cout << "You voted for " << candidates[index] << endl;
//    } 
//
//    void main() {
//        SetWindowVisible(true);
//        int dropList = CreateDropList(4, candidates, 2, &vote);
//        while (!WasWindowClosed()) { }         
//        DeleteControl(dropList);
//    }
//
int CreateDropList(int numItems, const char* const items[], int selected, functionptr_int onSelectionChanged);
int GetDropListSelection(int handle);
void SetDropListSelection(int handle, int selected);

// CreateSlider:
//    Creates a slider control. Each time the user drags the slider left/right,
//    the slider calls whatever function you passed as onValueChanged.
//    The 'value' of the slider is always in the interval [0.0, 1.0].
//
//    By default, sliders are horizontal. To make a slider appear vertical, 
//    just apply SetControlSize() with sizeY > sizeX.
//
// Example:
//    void setVolume(float volume) { 
//        int percent = (int)(volume*100);
//        cout << "The volume is now at " << percent << "%" << endl;
//    } 
//
//    void main() {
//        SetWindowVisible(true); 
//        int volumeSlider = CreateSlider(0.5f, &setVolume, false);
//        while (!WasWindowClosed()) { }
//        DeleteControl(volumeSlider);
//    }
//
int CreateSlider(float value, functionptr_float onValueChanged, bool isHorizontal=true);
float GetSliderValue(int handle);
void SetSliderValue(int handle, float value);

// SetControlPosition, SetControlSize:
//    These can be called for any type of control (buttons, drop-lists etc).
//    Position is relative to the top-left corner of the window, in pixels.
//
void SetControlPosition(int handle, int x, int y);
void SetControlSize(int handle, int sizeX, int sizeY);

// GetControlPosition, GetControlSize:
//    If you need to align controls manually (heaven forbid), then these functions
//    will assign to the variables whose addresses you pass in.
//
void GetControlPosition(int handle, int* x, int* y);
void GetControlSize(int handle, int* sizeX, int* sizeY);

// DeleteControl:
//    Removes the control from the window and deletes it.
//
void DeleteControl(int handle);

// SetWindowResizeCallback:
//    Every time the main window is resized, the window will call the
//    function that you passed in for onWindowResize. So if the user is 
//    dragging the corner of the window to resize, your function is called.
//
// Example:
//    void resized(int sizeX, int sizeY) {
//        cout << "Window is now " << sizeX << " by " << sizeY << " pixels." << endl;
//    }
//
//    void main() {
//        SetWindowResizeCallback(&resized);
//        SetWindowVisible(true);
//        while (!WasWindowClosed()) { }         
//    }
//
void SetWindowResizeCallback(functionptr_intint onWindowResize);
