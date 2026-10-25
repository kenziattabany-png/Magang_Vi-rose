#include <iostream>
using namespace std;

int main(){
    string nam;
    cin>>nam;

    char a;
    string NAM;
    for(int i = 0; i<nam.size(); i++){
        a = nam[i];

        if(a>='A' && a<='Z'){
            a+=32;
        }
        else if(a>='a' && a<='z'){
            a-=32;
        }

    NAM += a;
    }

    cout<<NAM;
    return 0;
}
