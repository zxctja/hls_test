#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


int GetResidualCost_C(int ctx0, uint16_t remapped_costs_[2][2][2][2], uint16_t coeffs[2], int n, int m) {
  const uint16_t (*costs)[2][2];
  costs = remapped_costs_[ctx0];
  int cost = 0;
  const uint16_t* t = costs[m][n];
  cost += t[0] + t[1] + coeffs[0] + coeffs[1];
  return cost;
}

int main(){
	uint16_t coeffs[2] = {1,2};
	uint16_t remapped_costs_[2][2][2][2];
	int i,j,k,l;
	for(i=0;i<2;i++){
		for(j=0;j<2;j++){
			for(k=0;k<2;k++){
				for(l=0;l<2;l++){
				remapped_costs_[l][k][j][i]=i+j+k+l;
				}
			}
		}
	}

	int cost = GetResidualCost_C(2,remapped_costs_,coeffs,2,1);
	printf("%x\n",cost);

	return 0;
}
