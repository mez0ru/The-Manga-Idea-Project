import { useState, useRef, useEffect, ReactElement } from 'react'
import Grid from '@mui/material/Grid';
import { BASE_URL } from '../api/axios';
import { useNavigate } from 'react-router-dom';
import { Page } from '../Viewer';
import { createPortal } from 'react-dom';

// const BASE_URL = 'http://localhost:4000'

interface Props {
    array: Page[];
    chapterId: number;
    onClick?: React.MouseEventHandler<HTMLImageElement>;
    currentId: number;
    setCurrentId: React.Dispatch<React.SetStateAction<number>>;
    // onClick: React.MouseEventHandler<HTMLImageElement>;
}

export default function DualPage({ chapterId, array, onClick, currentId, setCurrentId }: Props) {
    const navigate = useNavigate()

    const refImg1 = useRef<HTMLImageElement>(null);
    const refImg2 = useRef<HTMLImageElement>(null);

    // const [imgPool, setImgPool] = useState<ReactElement[]>([]);
    const [imgs, setImgs] = useState<Map<number, HTMLImageElement>>();

    const hideImages = () => {
        if (refImg1.current)
            refImg1.current.setAttribute('hidden', '');

        if (refImg2.current)
            refImg2.current.setAttribute('hidden', '');
    }

    const forward = () => {
        if (currentId !== array.length - 1) {
            hideImages();
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
            hideImages();
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

        console.log(`current id: ${currentId}, wide: ${array[currentId].isWide}`);

        const params = new URLSearchParams()
        if (currentId) {
            params.append("page", currentId.toString());
        } else {
            params.delete("page")
        }

        navigate({ search: params.toString() })

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

    const onLoad = (e: React.UIEvent<HTMLImageElement>) => {
        if (e.currentTarget === refImg1.current) {
            if (array[currentId].isWide || (currentId != array.length - 1 && array[currentId + 1].isWide) || currentId === 0 || currentId === array.length - 1 || (array.length > 2 && currentId === array.length - 2))
                e.currentTarget.removeAttribute("hidden");
            else if (refImg2.current?.complete) {
                refImg2.current?.removeAttribute("hidden");
                e.currentTarget.removeAttribute("hidden");
            }
        }
        else if (refImg1.current?.complete) {
            refImg1.current?.removeAttribute("hidden");
            e.currentTarget.removeAttribute("hidden");
        }
    }

    if (currentId !== 0) {
        if ((!array[currentId].isWide && (currentId != array.length - 1 && array[currentId + 1].isWide)) || currentId == array.length - 1 || (array.length > 2 && currentId == array.length - 2)) {
            return (<Grid container onClick={onImgClick}>
                <Grid item xs={6}>
                    <img style={{ height: '100vh', float: 'right' }} ref={refImg1} alt='1' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[currentId].i}`} onLoad={onLoad} hidden />
                </Grid>
                <Grid item xs={6}>
                </Grid>
            </Grid>);
        } else if (array[currentId].isWide) {
            return (
                <Grid container onClick={onImgClick}>
                    <Grid item xs={12}>
                        <img style={{ height: '100vh', display: 'block', margin: '0 auto' }} ref={refImg1} alt='1' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[currentId].i}`} onLoad={onLoad} hidden />
                    </Grid>
                </Grid>);
        }
        else {
            return (
                <Grid container onClick={onImgClick}>
                    <Grid item xs={6}>
                        <img style={{ height: '100vh', float: 'right' }} ref={refImg1} alt='1' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[currentId + 1].i}`} onLoad={onLoad} hidden />
                    </Grid>
                    <Grid item xs={6}>
                        <img style={{ height: '100vh', float: 'left' }} ref={refImg2} alt='2' src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[currentId].i}`} onLoad={onLoad} hidden />
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
                    <img style={{ height: '100vh', float: 'left' }} ref={refImg1} alt='1' onLoad={onLoad} src={`${BASE_URL}/api/v2/chapter/${chapterId}/page/${array[currentId].i}`} hidden />
                </Grid>
            </Grid>
        )
    }
}
