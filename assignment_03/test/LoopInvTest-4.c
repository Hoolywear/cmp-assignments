
/*
Sesto esempio: Caso con molti loop anidati.
*/
int foo6(int i1, int i2, int i3, int i4) {
while(1){
	if(i1 == 0){
		break;
	} else {
		while(1){
			int x1 = i1 + 1;
			if(i2 == 0){
				break;
			} else {
				while(1){
					int x2 = i2 + 2;
					if(i3 == 0){
						break;
					} else {
						while(1){
							int x3 = i3 + 3;
							if(i4 == 0){
								break;
							}
							i4--;
						}
					}
					i3--;
				}
			}
			i2--;
		}
	}
	i1--;
}
}
