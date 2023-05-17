import { useState, useRef, useEffect, ReactElement } from 'react'
import Grid from '@mui/material/Grid';
import { BASE_URL } from '../api/axios';
import { useNavigate } from 'react-router-dom';
import { Page } from '../Viewer';
import { createPortal } from 'react-dom';
import './DualPage.css';

// const BASE_URL = 'http://localhost:4000'

interface Props {
    array: Page[];
    chapterId: number;
    onClick?: React.MouseEventHandler<HTMLImageElement>;
    currentId: number;
    setCurrentId: React.Dispatch<React.SetStateAction<number>>;
    // onClick: React.MouseEventHandler<HTMLImageElement>;
}

const DualPageRender = (chapterId: number, id: number, array: Page[], onImgClick: React.MouseEventHandler<HTMLImageElement>) => {
    if (id !== 0) {
        if ((!array[id].isWide && (id != array.length - 1 && array[id + 1].isWide)) || id == array.length - 1 || (array.length > 2 && id == array.length - 2)) {
            return (<Grid container onClick={onImgClick}>
                <Grid item xs={6}>
                    <img style={{ height: '100vh', float: 'right' }} loading="lazy" alt='1' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[id].i}`} />
                </Grid>
                <Grid item xs={6}>
                </Grid>
            </Grid>);
        } else if (array[id].isWide) {
            return (
                <Grid container onClick={onImgClick}>
                    <Grid item xs={12}>
                        <img style={{ height: '100vh', display: 'block', margin: '0 auto' }} loading="lazy" alt='1' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[id].i}`} />
                    </Grid>
                </Grid>);
        }
        else {
            return (
                <Grid container onClick={onImgClick}>
                    <Grid item xs={6}>
                        <img style={{ height: '100vh', float: 'right' }} loading="lazy" alt='1' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[id].i}`} />
                    </Grid>
                    <Grid item xs={6}>
                        <img style={{ height: '100vh', float: 'left' }} loading="lazy" alt='2' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[id + 1].i}`} />
                    </Grid>
                </Grid>);
        }
    }
    else {
        return (
            <Grid container onClick={onImgClick}>
                <Grid item xs={6}>
                </Grid>
                <Grid item xs={6}>
                    <img style={{ height: '100vh', float: 'left' }} loading="lazy" alt='1' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[id].i}`} />
                </Grid>
            </Grid>
        )
    }
}

export default function DualPage({ chapterId, array, onClick, currentId, setCurrentId }: Props) {
    const navigate = useNavigate()

    const refImg1 = useRef<HTMLImageElement>(null);
    const refImg2 = useRef<HTMLImageElement>(null);

    // const [imgPool, setImgPool] = useState<ReactElement[]>([]);
    const [imgs, setImgs] = useState<Map<number, HTMLImageElement>>();

    const forward = () => {
        if (currentId !== array.length - 1) {
            setCurrentId(x => {
                // refImg1.current!.hidden = true;
                if ((currentId !== array.length - 1 && array[x + 1].isWide) || currentId !== array.length - 1 || array[x].isWide || x === 0) {
                    return x + 1;
                }
                else {
                    return x + 2;
                }
            });

        }
    }

    const backward = () => {
        if (currentId !== 0) {
            setCurrentId(x => {
                // refImg1.current!.hidden = true;
                if (array[x - 1].isWide || currentId <= 2) {
                    // searchParams.set("page", `${x - 1}`);
                    return x - 1;
                }
                else {
                    // if (refImg2.current)
                    //     refImg2.current.hidden = true;
                    return x - 2;
                }
            })
        }
    }

    const onImgClick = (e: React.MouseEvent<HTMLImageElement>) => {
        if (e.clientY > e.currentTarget.clientHeight / 1.5) {
            // forward
            forward();
        } else if (e.clientY < e.currentTarget.clientHeight / 3) {
            // previous
            backward();
        } else {
            if (onClick)
                onClick(e);
        }
    }

    useEffect(() => {
        let correctOffset = 0;
        let firstPage = true;

        // mindfuck algorithm to find the correct page structure. If there's a simplier way to do it, please share it.
        for (let i = 0; i < currentId; i++) {
            if (i === 0 || array.length > 2 && i === array.length - 1 || i === array.length - 2) {
                correctOffset++;
            } else if (firstPage && array[i + 1].isWide) {
                correctOffset++;
                firstPage != firstPage;
            } else if (!firstPage && array[i].isWide) {
                correctOffset++;
                firstPage != firstPage;
            } else if (array[i].isWide) {
                correctOffset++;
            } else {
                correctOffset += 2;
                i++;
            }
        }

        if (correctOffset > currentId) {
            setCurrentId(correctOffset);
            return;
        }

        // console.log(`current id: ${currentId}, wide: ${array[currentId].isWide}`);

        // Change URL based on page index
        const params = new URLSearchParams()
        if (currentId) {
            params.append("page", currentId.toString());
        } else {
            params.delete("page")
        }

        navigate({ search: params.toString() })

        // register keyboard events
        const keyDownEvent = (e: KeyboardEvent) => {
            if (e.key === 'ArrowDown') {
                forward();
            } else if (e.key === 'ArrowUp') {
                backward();
            }
        }

        document.addEventListener('keydown', keyDownEvent);

        return () => {
            document.removeEventListener('keydown', keyDownEvent);
        }
    }, [currentId])

    return DualPageRender(chapterId, currentId, array, onImgClick);
}
