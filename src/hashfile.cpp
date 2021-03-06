
/*
 * This file is part of Codecrypt.
 *
 * Codecrypt is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * Codecrypt is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Codecrypt. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hashfile.h"

#include <map>
using namespace std;

#include "rmd_hash.h"
#include "sha_hash.h"
#include "tiger_hash.h"
#include "cube_hash.h"

#include "iohelpers.h"

/*
 * helper -- size measurement is a kindof-hash as well
 */

class size64proc : public hash_proc
{
	uint64_t s;

	uint size() {
		return 8;
	}

	void init() {
		s = 0;
	}

	void eat (const std::vector<byte>&a) {
		s += a.size();
	}

	std::vector<byte> finish() {
		std::vector<byte> r;
		r.resize (8, 0);
		for (int i = 0; i < 8; ++i) {
			r[i] = s & 0xff;
			s >>= 8;
		}
		return r;
	}
};

/*
 * list of hash functions availabel
 */

typedef map<string, hash_proc*> hashmap;

void fill_hashmap (hashmap&t)
{
#if HAVE_CRYPTOPP==1
	static rmd128proc rh;
	t["RIPEMD128"] = &rh;
	static tiger192proc th;
	t["TIGER192"] = &th;
	static sha256proc sh256;
	t["SHA256"] = &sh256;
	static sha512proc sh512;
	t["SHA512"] = &sh512;
#endif //HAVE_CRYPTOPP
	static cube512proc c512;
	t["CUBE512"] = &c512;
	static size64proc sh;
	t["SIZE64"] = &sh;
}

bool hashfile::create (istream&in)
{
	hashes.clear();

	hashmap hm;
	fill_hashmap (hm);

	for (hashmap::iterator i = hm.begin(), e = hm.end(); i != e; ++i)
		i->second->init();

	std::vector<byte> buf;
	buf.resize (8192);

	for (;;) {
		in.read ( (char*) & (buf[0]), 8192);
		if (in)
			for (hashmap::iterator i = hm.begin(), e = hm.end();
			     i != e; ++i)
				i->second->eat (buf);
		else if (in.eof() ) {
			buf.resize (in.gcount() );
			for (hashmap::iterator i = hm.begin(), e = hm.end();
			     i != e; ++i) {
				i->second->eat (buf);
				hashes[i->first] = i->second->finish();
			}
			return true;
		} else return false;
	}
}

int hashfile::verify (istream&in)
{
	hashmap hm_all, hm;
	fill_hashmap (hm_all);

	for (hashes_t::iterator i = hashes.begin(), e = hashes.end(); i != e; ++i)
		if (hm_all.count (i->first) )
			hm[i->first] = hm_all[i->first];


	if (hm.empty() ) {
		err ("notice: no verifiable hash found in hashfile");
		return 2;
	}

	for (hashmap::iterator i = hm.begin(), e = hm.end(); i != e; ++i)
		i->second->init();

	std::vector<byte> buf;
	buf.resize (8192);

	for (;;) {
		in.read ( (char*) & (buf[0]), 8192);
		if (in)
			for (hashmap::iterator i = hm.begin(), e = hm.end();
			     i != e; ++i)
				i->second->eat (buf);
		else if (in.eof() ) {
			buf.resize (in.gcount() );
			for (hashmap::iterator i = hm.begin(), e = hm.end();
			     i != e; ++i) {
				i->second->eat (buf);
			}
			break;
		} else return 1;
	}

	int ok = 0, failed = 0;
	for (hashes_t::iterator i = hashes.begin(), e = hashes.end();
	     i != e; ++i) {
		if (!hm.count (i->first) ) {
			err ("hash verification: :-/ "
			     << i->first << " not supported");
			continue;
		}
		if (i->second == hm[i->first]->finish() ) {
			++ok;
			err ("hash verification: ;-) "
			     << i->first << " is GOOD");
		} else {
			++failed;
			err ("hash verification: :-( "
			     << i->first << " is BAD");
		}
	}

	if (failed) return 3;
	if (ok) return 0;
	return 2;
}
