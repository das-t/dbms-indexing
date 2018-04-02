#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
using namespace std;

#define ull unsigned long long
#define datatype int
#define N 4
#define NODE_SIZE (N * 8 + (N-1) * sizeof(datatype))

typedef struct {
	bool isLeaf;
	int keyCount;
	ull offset[N];
	datatype key[N-1];
} Node;

typedef struct {
	int keyCount;
	ull offset[N+1];
	datatype key[N];
} auxNode;

typedef struct {
	ull node;
	ull parent;
	bool end;
} Parent;

Parent* parentsList = (Parent*)calloc(25*N, sizeof(Parent));

void write_index_metadata_to_file(FILE* indexFile) {
	ull no_records = 0; // Initially no records
	int size_of_node = NODE_SIZE; // Size of each node in the tree
	ull root_node_number = 0; // Initially there is no root

	fwrite(&no_records, sizeof(ull), 1, indexFile);
	fwrite(&size_of_node, sizeof(int), 1, indexFile);
	fwrite(&root_node_number, sizeof(ull), 1, indexFile);
}

ull getRootNodeNumber(FILE* indexFile) {
	ull root_node_number;
	fseek(indexFile, sizeof(ull) + sizeof(int), SEEK_SET);
	fread(&root_node_number, sizeof(ull), 1, indexFile);
	return root_node_number;
}

long getPositionToSeekInFile(ull record_number) {
	// Sizeof(root_node_number) + sizeof(no_records) + sizeof(size_of_node_variable) +  
	long position = 2 * sizeof(ull) + sizeof(int) + (record_number - 1) * NODE_SIZE;
	return position;
}

void write_record_to_file_from_variable(Node** node, size_t size, FILE* indexFile, ull record_number) {
	long position = getPositionToSeekInFile(record_number);

	// Seek to the position in the file from beginning
	fseek(indexFile, position, SEEK_SET);

	// Write record to file at the seeked position from the variable
	fwrite(*node, size, 1, indexFile);
}

void load_record_from_file_into_variable(Node** node, size_t size, FILE* indexFile, ull record_number) {
	long position = getPositionToSeekInFile(record_number);
	// Seek to the positon in file from beginning
	fseek(indexFile, position, SEEK_SET);
	// Read record from file at the seeked position into the variable
	fread(*node, size, 1, indexFile);
}

ull update_no_of_records_in_index_file(FILE* indexFile, int count) {
	ull no_records;
	fread(&no_records, sizeof(ull), 1, indexFile);
	no_records += count;
	fwrite(&no_records, sizeof(ull), 1, indexFile);
}

ull get_no_of_records_in_index_file(FILE* indexFile) {
	ull no_records;
	fread(&no_records, sizeof(ull), 1, indexFile);
	return no_records;
}

void update_root_node_number(FILE* indexFile, ull newNumber) {
	fseek(indexFile, sizeof(ull) + sizeof(int), SEEK_SET);
	fwrite(&newNumber, sizeof(ull), 1, indexFile);
}

ull findUtil(Node* root, datatype key, FILE* file) {
	// Set result as root node
	Node* result = root;
	ull current_node_number = getRootNodeNumber(indexFile);
	ull previous_node_number = 0; // No parent for root

	// Save parent information for root node in parentsList
	int i = 0;
	parentsList[i].node = current_node_number;
	parentsList[i].parent = previous_node_number;
	parentsList[i].end = FALSE;

	// Create a temporary memory for storing the retrieved node from file
	Node* temp = (Node*)calloc(1, sizeof(Node));

	// While result is not a leaf node
	while (result->isLeaf == FALSE) {
		// Find smallest number i such that result->key[i] <= key
		int j = 0;
		while (j < result->keyCount && result->key[j] < key)
			j++;
		if (j == result->keyCount) {
			previous_node_number = current_node_number;
			current_node_number = result->offset[j];

			// Save parent information
			++i;
			parentsList[i].node = current_node_number;
			parentsList[i].parent = previous_node_number;
			parentsList[i].end = FALSE;

			// Get the current node from file and store it in temp
			load_record_from_file_into_variable(&temp, sizeof(Node), indexFile, current_node_number);

			// Update result
			result = temp;
		}
		else if (key == result->key[j]) {
			previous_node_number = current_node_number;
			current_node_number = result->offset[j+1];

			// Save parent information
			++i;
			parentsList[i].node = current_node_number;
			parentsList[i].parent = previous_node_number;
			parentsList[i].end = FALSE;

			// Get the current node from file and store it in temp
			load_record_from_file_into_variable(&temp, sizeof(Node), indexFile, current_node_number);

			// Update result
			result = temp;
		}
		else {
			previous_node_number = current_node_number;
			current_node_number = result->offset[j];

			// Save parent information
			++i;
			parentsList[i].node = current_node_number;
			parentsList[i].parent = previous_node_number;
			parentsList[i].end = FALSE;

			// Get the current node from file and store it in temp
			load_record_from_file_into_variable(&temp, sizeof(Node), indexFile, current_node_number);

			// Update result
			result = temp;
		}
	}

	// Set the last node in parentsList as end
	parentsList[i].end = TRUE;

	free(temp);

	// Return the leaf node's record number in the index file which may contain the given key
	return current_node_number;
}

ull indexFindUtil(Node* root, datatype key, FILE* file) {
	// Get the record number of the leaf node which may contain the given key
	ull record_number = findUtil(root, key, file);

	// Create memory for leafNode
	Node* leafNode = (Node*)calloc(1, sizeof(Node));

	// Retrieve the node from the file
	load_record_from_file_into_variable(&leafNode, sizeof(Node), file, record_number);

	ull record_offset;

	// Find the least i such that leafNode->key[i] = key
	int i = 0;
	while (i < leafNode->keyCount && leafNode->key[i] < key)
		i++;
	if (i == leafNode->keyCount || leafNode->key[i] > key)
		record_offset = 0; // Record not found
	else
		record_offset = leafNode->offset[i]; // Record number in the database file

	return record_offset;
}

ull indexFind(datatype key, char* fileName) {
	FILE* indexFile = fopen(fileName, "rb+");

	if (NULL == indexFile) {
		return 0; // File does not exist or error opening file
	}

	// Get the root node number from the file
	ull root_node_number = getRootNodeNumber(indexFile);

	// Create memory for root node and extract that node from the file
	Node* root;
	if (root_node_number == 0) {
		root = NULL;
	}
	else {
		root = (Node*)calloc(1, sizeof(Node));
		load_record_from_file_into_variable(&root, sizeof(Node), indexFile, root_node_number);
	}

	ull record_offset = indexFindUtil(root, key, indexFile);

	return record_offset;
}

ull getParent(ull node) {
	int i = 0;
	while (parentsList[i].end == FALSE) {
		if (parentsList[i].node == node)
			return parentsList[i].parent;
		++i;
	}
	if (parentsList[i].node == node)
		return parentsList[i].parent;
	return 0; // Node not found
}

bool insertInLeafUtil(Node* leaf, datatype key, ull recordOffset) {
	if (key < leaf->key[0]) {
		// Move all the keys and offsets one positions to the right
		int i;
		for (i = leaf->keyCount-1; i >= 0; i--) {
			leaf->key[i+1] = leaf->key[i];
			leaf->offset[i+1] = leaf->offset[i];
		}
		// Now insert given key and recordOffset at the first locations
		leaf->key[0] = key;
		leaf->offset[0] = recordOffset;
	}
	else {
		// Find largest i such that leaf->key[i] < key and insert after that
		int i = 0;
		while (i < leaf->keyCount && leaf->key[i] < key)
			i++;
		if (i == leaf->keyCount) {
			leaf->key[i] = key;
			leaf->offset[i] = recordOffset;
		}
		else {
			int j;
			for (j = leaf->keyCount; j > i; j--) {
				leaf->key[j] = leaf->key[j-1];
				leaf->offset[j] = leaf->offset[j-1];
			}
			leaf->key[i] = key;
			leaf->offset[i] = recordOffset;
		}
	}
	// Increment keyCount by 1
	leaf->keyCount++;
}

bool insertInLeaf(ull leaf_node_number, datatype key, ull recordOffset, FILE* indexFile) {
	// Create memory for leaf node
	Node* leaf = (Node*)calloc(1, sizeof(Node));

	// Load from file the node at leaf_node_number into leaf
	load_record_from_file_into_variable(&leaf, sizeof(Node), indexFile, leaf_node_number);

	if (key < leaf->key[0]) {
		// Move all the keys and offsets one position to the right
		int i;
		for (i = leaf->keyCount-1; i >= 0; i--) {
			leaf->key[i+1] = leaf->key[i];
			leaf->offset[i+1] = leaf->offset[i];
		}
		// Now insert given key and recordOffset at the first locations
		leaf->key[0] = key;
		leaf->offset[0] = recordOffset;

		// Increment keyCount by 1
		leaf->keyCount++;

		// Write the modified leaf back into file
		write_record_to_file_from_variable(&leaf, sizeof(Node), indexFile, leaf_node_number);
	}
	else {
		// Find largest i such that leaf->key[i] < key and insert after that
		int i = 0;
		while (i < leaf->keyCount && leaf->key[i] < key)
			i++;
		if (i == leaf->keyCount) {
			leaf->key[i] = key;
			leaf->offset[i] = recordOffset;
		}
		else {
			int j;
			for (j = leaf->keyCount; j > i; j--) {
				leaf->key[j] = leaf->key[j-1];
				leaf->offset[j] = leaf->offset[j-1];
			}
			leaf->key[i] = key;
			leaf->offset[i] = recordOffset;
		}
		// Increment keyCount by 1
		leaf->keyCount++;

		// Write the modified leaf back into file
		write_record_to_file_from_variable(&leaf, sizeof(Node), indexFile, leaf_node_number);
	}

	free(leaf);
}

bool insertInParent(Node** root_ptr, Node* nodeLeft, datatype key, Node* nodeRight) {
	// If nodeLeft is the root of the tree
	if (nodeLeft == *root_ptr) {
		// Create a new node containing pointers nodeLeft, nodeRight and value key
		Node* newNode = (Node*)calloc(1, sizeof(Node));
		newNode->offset[0] = nodeLeft;
		newNode->offset[1] = nodeRight;
		newNode->key[0] = key;
		newNode->keyCount = 1;
		newNode->isLeaf = FALSE;
		// Make newNode the root of the tree
		*root_ptr = newNode;
	}
	else {
		ull parent = getParent(nodeLeft);
		// If parent has less than N pointers
		if (parent->keyCount < N-1) {
			// Insert key, nodeRight in parent just after nodeLeft
			// Find i for which parent->offset[i] = nodeLeft
			int i;
			for (i = 0; i <= parent->keyCount; i++)
				if (parent->offset[i] == nodeLeft) break;
			// Move right all keys and offsets after i by one position
			int j;
			for (j = parent->keyCount; j > i; j--) {
				parent->offset[j+1] = parent->offset[j];
				parent->key[j] = parent->key[j-1];
			}
			// Insert (key, nodeRight) after nodeLeft
			parent->key[i] = key;
			parent->offset[i+1] = nodeRight;
			parent->keyCount++;
		}
		else {
			// Else split parent and recurse
			// Create auxNode
			auxNode* temp = (auxNode*)calloc(1, sizeof(auxNode));
			// Copy parent to temp
			int i;
			for (i = 0; i < parent->keyCount; i++) {
				temp->offset[i] = parent->offset[i];
				temp->key[i] = parent->key[i];
			}
			temp->offset[i] = parent->key[i];
			temp->keyCount = parent->keyCount;
			// Insert (key, nodeRight) into temp just after nodeLeft
			// Find i for which temp->offset[i] = nodeLeft
			for (i = 0; i <= temp->keyCount; i++)
				if (temp->offset[i] == nodeLeft) break;
			// Move right all keys and offsets after i by one position
			int j;
			for (j = temp->keyCount; j > i; j--) {
				temp->offset[j+1] = temp->offset[j];
				temp->key[j] = temp->key[j-1];
			}
			// Insert (key, nodeRight) after nodeLeft
			temp->key[i] = key;
			temp->offset[i+1] = nodeRight;
			temp->keyCount++;
			// Create new node newParent
			Node* newParent = (Node*)calloc(1, sizeof(newParent));
			// Copy temp->offset[0] to temp->offset[ceil(n/2)-1] into parent
			for (i = 0; i < ceil(N/2)-1; i++) {
				parent->offset[i] = temp->offset[i];
				parent->key[i] = temp->key[i];
			}
			parent->offset[i] = temp->offset[i];
			parent->keyCount = ceil(N/2) - 1;
			// Let newKey = temp->key[ceil(n/2)-1]
			datatype newKey = temp->key[i];
			// Copy temp->offset[ceil(n/2)] to temp->offset[n] into newParent
			for (i = ceil(N/2); i < N; i++) {
				newParent->offset[i-ceil(N/2)] = temp->offset[i];
				newParent->key[i-ceil(N/2)] = temp->key[i];
			}
			newParent->offset[i-ceil(N/2)] = temp->offset[i];
			newParent->keyCount = N - ceil(N/2);
			newParent->isLeaf = FALSE;
			// Call insert_in_parent recursively for parent and newParent with newKey
			insertInParent(root_ptr, parent, newKey, newParent);
		}
	}
}

bool insertUtil(Node* root, datatype key, ull recordOffset, FILE* indexFile) {
	if (root == NULL) {
		// If tree is empty, create an empty leaf node which is also the root
		Node* newNode = (Node*)calloc(1, sizeof(Node));
		newNode->isLeaf = TRUE;
		newNode->key[0] = key;
		newNode->offset[0] = recordOffset;
		newNode->offset[N-1] = 0;
		newNode->keyCount = 1;

		// Get number of records in index file
		ull no_records = get_no_of_records_in_index_file(indexFile);

		// Insert new node at the last of the index file
		write_record_to_file_from_variable(&newNode, sizeof(Node), indexFile, no_records+1);

		// Increase number of records count by 1 in the file
		update_no_of_records_in_index_file(indexFile, 1);

		// Make this newNode the root, i.e. update the root number in index file
		update_root_node_number(indexFile, no_records+1);

		free(newNode);
	}
	else {
		// Else find the leaf node that should contain the given key
		ull leaf_node_number = findUtil(root, key, indexFile);

		// Create memory for leafNode
		Node* leafNode = (Node*)calloc(1, sizeof(Node));

		// Load from file the node at leaf_node_number into leafNode
		load_record_from_file_into_variable(&leafNode, sizeof(Node), indexFile, leaf_node_number);

		// If this leaf node contains n-1 key values then insert in this leaf node
		if (leafNode->keyCount < N-1) {
			insertInLeaf(leaf_node_number, key, recordOffset, indexFile);
		}
		else {
			// Else leafNode had n-1 key values already, split it
			// Copy leafNode into auxNode which can hold n (pointer, key) values
			auxNode* temp = (auxNode*)calloc(1, sizeof(auxNode));
			int i;
			for (i = 0; i < N-1; i++) {
				temp->key[i] = leafNode->key[i];
				temp->offset[i] = leafNode->offset[i];
			}
			temp->keyCount = N-1;
			// Now insert into temp the given key and offset
			insertInLeafUtil(temp, key, recordOffset);

			// Create new leaf node
			Node* newLeafNode = (Node*)calloc(1, sizeof(Node));
			ull new_leaf_node_number = get_no_of_records_in_index_file(indexFile) + 1;

			// Make newLeafNode as next node to leafNode
			newLeafNode->offset[N-1] = leafNode->offset[N-1];
			leafNode->offset[N-1] = new_leaf_node_number;

			// Copy temp->offset[0] to temp->key[ceil(N/2)-1] into leafNode starting at leafNode->offset[0]
			for (i = 0; i <= ceil(N/2)-1; i++) {
				leafNode->key[i] = temp->key[i];
				leafNode->offset[i] = leafNode->offset[i];
			}
			leafNode->keyCount = ceil(N/2);

			// Copy temp->offset[ceil(n/2)] to temp->key[n-1] into newLeafNode starting at newLeafNode->offset[0]
			for (i = ceil(N/2); i <= N-1; i++) {
				newLeafNode->offset[i-ceil(N/2)] = temp->offset[i];
				newLeafNode->key[i-ceil(N/2)] = temp->key[i];
			}
			newLeafNode->keyCount = N - ceil(N/2);
			newLeafNode->isLeaf = TRUE;

			// Write the modified leafNode and newLeafNode back into file
			write_record_to_file_from_variable(&leafNode, sizeof(Node), indexFile, leaf_node_number);
			write_record_to_file_from_variable(&newLeafNode, sizeof(Node), indexFile, new_leaf_node_number);

			// Increase number of records count by 1 in file
			update_no_of_records_in_index_file(indexFile, 1);

			datatype newKey = newLeafNode->key[0];

			// Free memory
			free(temp);
			free(leafNode);
			free(newLeafNode);

			// Insert in parent the smallest key value of newLeafNode
			insertInParent(root, leaf_node_number, newKey, new_leaf_node_number);
		}
	}
}

bool indexInsert(datatype key, ull recordOffset, char* fileName) {
	FILE *indexFile = fopen(fileName, "wb+x");

	// If file already exists, open in read/update more
	if (indexFile == NULL) {
		indexFile = fopen(fileName, "rb+");
		assert(indexFile != NULL);
	}
	else {
		// If file does not already exist, write metadata to newly created file
		write_index_metadata_to_file(indexFile);
	}

	// Get the root node number from the file
	ull root_node_number = getRootNodeNumber(indexFile);

	// Create memory for root node and extract that node from the file
	Node* root;
	if (root_node_number == 0) {
		root = NULL;
	}
	else {
		root = (Node*)calloc(1, sizeof(Node));
		load_record_from_file_into_variable(&root, sizeof(Node), indexFile, root_node_number);
	}

	insertUtil(root, key, recordOffset);
}

int main() {

	return 0;
}