#include <iostream>
using namespace std;


int main() {
    int p,q;
    cin>>p>>q;
    
    int tot;
    tot = (p*p)+(q*q)+11+9;
    
    int berhasil;
    if(tot % 4 == 0){
        berhasil = tot/4;
        cout<<berhasil;
    }
    else{
        cout<<0;
    }
    return 0;
}
