/*
 * Copyright (C) 2011  Christos Tsilopoulos, Mobile Multimedia Laboratory, 
 * Athens University of Economics and Business 
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 3 as published by the Free Software Foundation.
 *
 * See LICENSE and COPYING for more details.
 */

package eu.pursuit.core;

public class ForwardIdentifier {
	private final ByteIdentifier linkID;

	public ForwardIdentifier(ByteIdentifier id) {
		Util.checkNull(id);
		this.linkID = id;
	}

	public ByteIdentifier getLinkID() {
		return linkID;
	}

	public byte[] toByteArray() {
		return linkID.getId();
	}
}
